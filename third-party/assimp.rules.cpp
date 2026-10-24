#include "builder/rules.hpp"

// assimp 6.0.2 as rndr's CMake configures it: static, import only, the OBJ and glTF importers, its
// bundled zlib. The paths are relative to the release archive's root; the headers assimp's CMake
// generates at configure time sit next to this file, in assimp/.

BUILDER_MODULE(Zlib, m, t)
{
    m.public_includes = {"contrib/zlib", BUILDER_HERE("assimp/zlib")};
    m.sources.AddDir("contrib/zlib");
    m.warnings = Warnings::Off;
    if (t.toolchain.Is(Compiler::Msvc))
    {
        m.private_defines = {"_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE"};
    }
    else
    {
        m.private_defines = {"_LARGEFILE64_SOURCE=1"};
    }
}

BUILDER_MODULE(Assimp, m, t)
{
    m.public_includes = {"include", BUILDER_HERE("assimp/include")};
    m.private_includes = {".",
                          "code",
                          "contrib",
                          "contrib/pugixml/src",
                          "contrib/utf8cpp/source",
                          "contrib/rapidjson/include",
                          "contrib/unzip",
                          "contrib/openddlparser/include"};
    m.private_deps = {"Zlib"};
    m.cxx_standard = 17;
    m.warnings = Warnings::Off;
    // assimp throws and catches inside itself; its C interface, which rndr calls, returns null instead.
    m.exceptions = true;

    for (const char* dir : {"code/Common", "code/CApi", "code/Geometry", "code/Material", "code/PostProcessing", "code/AssetLib/STEPParser",
                            "code/AssetLib/glTFCommon", "code/AssetLib/Obj", "code/AssetLib/glTF", "code/AssetLib/glTF2",
                            "contrib/unzip", "contrib/clipper", "contrib/openddlparser/code", "contrib/Open3DGC",
                            "contrib/poly2tri/poly2tri/common", "contrib/poly2tri/poly2tri/sweep", "contrib/pugixml/src"})
    {
        // Exporting is compiled out, so its two sources are left out as upstream does.
        m.sources.AddDir(dir).Exclude("code/Common/Exporter.cpp").Exclude("code/CApi/AssimpCExport.cpp");
    }
    m.headers.AddDir("include/assimp", Recurse::Yes);

    m.private_defines = {"ASSIMP_BUILD_NO_EXPORT",
                         "ASSIMP_BUILD_NO_M3D_IMPORTER",
                         "ASSIMP_BUILD_NO_M3D_EXPORTER",
                         "ASSIMP_BUILD_NO_VRML_IMPORTER",
                         "ASSIMP_BUILD_NO_C4D_IMPORTER",
                         "ASSIMP_IMPORTER_GLTF_USE_OPEN3DGC=1",
                         "RAPIDJSON_HAS_STDSTRING=1",
                         "RAPIDJSON_NOMEMBERITERATORCLASS",
                         "OPENDDLPARSER_BUILD",
                         "OPENDDL_STATIC_LIBARY",
                         "P2T_STATIC_EXPORTS"};
    // Every importer but OBJ and glTF, which ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF turns off.
    for (const char* importer : {"AMF", "3DS",    "AC",      "ASE",   "ASSBIN", "B3D",  "BVH",      "COLLADA", "DXF", "CSM", "HMP",
                                 "IRRMESH", "IQM", "IRR",    "LWO",   "LWS",    "MD2",  "MD3",      "MD5",     "MDC", "MDL", "NFF",
                                 "NDO", "OFF",     "OGRE",   "OPENGEX", "PLY",  "MS3D", "COB",      "BLEND",   "IFC", "XGL", "FBX",
                                 "Q3D", "Q3BSP",   "RAW",    "SIB",   "SMD",    "STL",  "TERRAGEN", "3D",      "USD", "X",   "X3D",
                                 "3MF", "MMD"})
    {
        m.private_defines.push_back(std::string("ASSIMP_BUILD_NO_") + importer + "_IMPORTER");
    }

    if (t.platform.Is(Platform::Windows))
    {
        Append(m.private_defines, {"WIN32_LEAN_AND_MEAN", "UNICODE", "_UNICODE"});
    }
    if (t.toolchain.Is(Compiler::Msvc))
    {
        Append(m.private_defines, {"_SCL_SECURE_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"});
        m.private_compile_flags = {"/bigobj"};
    }
    else
    {
        m.private_compile_flags = {"-fno-strict-aliasing"};
    }
    if (t.platform.Is(Platform::Linux))
    {
        // Open3DGC's timer.
        m.libs = {"rt"};
    }
}
