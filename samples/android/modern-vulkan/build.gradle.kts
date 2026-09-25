import java.net.URI

plugins {
    id("com.android.application")
}

// samples/android/modern-vulkan -> the repository, whose CMakeLists.txt builds the library and whose assets/ holds the model.
val repositoryRoot: File = rootDir.resolve("../..").canonicalFile
// Where rndr_compile_shader writes the SPIR-V the device loads in place of Slang source. Named here so it can be
// packaged from here; the CMake build directory is Gradle's and has a hash in its path.
val spirvDirectory: File = layout.buildDirectory.dir("generated/rndr-spirv").get().asFile
// The validation layer, which the loader takes from a debuggable app's own libraries. Fetched from the Khronos
// release that matches the Vulkan headers rndr pins, rather than committed.
val validationLayerVersion = "1.4.335.0"
val validationLayerDirectory: File = layout.buildDirectory.dir("generated/validation-layer").get().asFile

android {
    namespace = "dev.rndr.modernvulkan"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "dev.rndr.modernvulkan"
        // Nothing lower has a Vulkan 1.3 driver, and the manifest refuses a device without one anyway.
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += "arm64-v8a"
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DRNDR_FORGE=ON",
                    "-DRNDR_FORGE_VALIDATION=ON",
                    "-DRNDR_ASSIMP=ON",
                    "-DRNDR_KTX=OFF",
                    "-DRNDR_BUILD_TESTS=OFF",
                    "-DRNDR_BUILD_SAMPLES=ON",
                    "-DRNDR_SPIRV_DIR=${spirvDirectory.invariantSeparatorsPath}",
                )
                targets += "modern-vulkan"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = repositoryRoot.resolve("CMakeLists.txt")
            // rndr needs 3.28, and AGP picks 3.22 unless told. Fetched into the SDK on first use.
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            assets.directories += listOf(
                repositoryRoot.resolve("assets/sample-models/Suzanne/glTF").path,
                spirvDirectory.resolve("modern-vulkan").path,
            )
        }
        getByName("debug") {
            jniLibs.directories += validationLayerDirectory.path
        }
    }
}

val fetchValidationLayer = tasks.register("fetchValidationLayer") {
    val archive = layout.buildDirectory.file("downloads/android-binaries-$validationLayerVersion.zip").get().asFile
    val url = "https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/vulkan-sdk-$validationLayerVersion/" +
        "android-binaries-$validationLayerVersion.zip"
    val output = validationLayerDirectory
    outputs.dir(output)
    doLast {
        if (!archive.exists()) {
            archive.parentFile.mkdirs()
            URI(url).toURL().openStream().use { input -> archive.outputStream().use { input.copyTo(it) } }
        }
        val layer = zipTree(archive).matching { include("**/arm64-v8a/libVkLayer_khronos_validation.so") }.singleFile
        val destination = output.resolve("arm64-v8a")
        destination.mkdirs()
        layer.copyTo(destination.resolve(layer.name), overwrite = true)
    }
}

// The SPIR-V is written by the native build, which AGP does not run before it merges the assets.
tasks.configureEach {
    when (name) {
        "mergeDebugAssets" -> dependsOn("externalNativeBuildDebug")
        "mergeReleaseAssets" -> dependsOn("externalNativeBuildRelease")
        "mergeDebugJniLibFolders" -> dependsOn(fetchValidationLayer)
    }
}
