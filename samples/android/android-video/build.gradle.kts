import java.net.URI

plugins {
    id("com.android.application")
}

// samples/android/android-video -> the repository, whose CMakeLists.txt builds the library and the sample.
val repositoryRoot: File = rootDir.resolve("../..").canonicalFile
// Where rndr_compile_shader writes the SPIR-V the device loads in place of Slang source.
val spirvDirectory: File = layout.buildDirectory.dir("generated/rndr-spirv").get().asFile
// The validation layer, which the loader takes from a debuggable app's own libraries, as modern-vulkan fetches it.
val validationLayerVersion = "1.4.335.0"
val validationLayerDirectory: File = layout.buildDirectory.dir("generated/validation-layer").get().asFile
// Which ABIs to build, comma separated: arm64-v8a for a phone, x86_64 for the emulator. `-Prndr.abis=x86_64`, or both.
val abis: List<String> = (findProperty("rndr.abis") as String? ?: "arm64-v8a").split(",").map { it.trim() }.filter { it.isNotEmpty() }

android {
    namespace = "dev.rndr.androidvideo"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "dev.rndr.androidvideo"
        // AHardwareBuffer_lockPlanes and AHardwareBuffer_isSupported are 29, as is modern-vulkan's floor.
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += abis
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
                targets += "android-video"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = repositoryRoot.resolve("CMakeLists.txt")
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            this.java.directories += repositoryRoot.resolve("src/platform/java").path
            assets.directories += spirvDirectory.resolve("android-video").path
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
        for (abi in abis) {
            val layer = zipTree(archive).matching { include("**/$abi/libVkLayer_khronos_validation.so") }.singleFile
            val destination = output.resolve(abi)
            destination.mkdirs()
            layer.copyTo(destination.resolve(layer.name), overwrite = true)
        }
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
