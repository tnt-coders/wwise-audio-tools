cmake_minimum_required(VERSION 3.24)

# Required for MSVC to cooperate with Conan 2.X+
cmake_policy(SET CMP0091 NEW)

project(WwiseAudioTools VERSION 1.0.0 LANGUAGES C CXX)

option(BUILD_CLI "Build command-line interface" ON)
option(PACKED_CODEBOOKS_AOTUV "Use data from packed_codebooks_aoTuV_603.bin instead of regular packed_codebooks.bin" ON)

find_package(PackageBuilder REQUIRED)
find_package(Catch2 REQUIRED)
find_package(kaitai_struct_cpp_stl_runtime REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)

include(PackageBuilder)

# Create the package
package_create()

set(SOURCE
    src/ww2ogg/codebook.cpp
    src/ww2ogg/crc.c
    src/ww2ogg/ww2ogg.cpp
    src/ww2ogg/wwriff.cpp
    src/revorb/revorb.cpp

    src/kaitai/structs/bnk.cpp
    src/kaitai/structs/vlq.cpp
    src/kaitai/structs/wem.cpp

    src/bnk.cpp
    src/wwtools.cpp)

if(PACKED_CODEBOOKS_AOTUV)
    message("PACKED_CODEBOOKS_AOTUV set as on. Using data from packed_codebooks_aoTuV_603.bin.")
    list(APPEND SOURCE src/ww2ogg/packed_codebooks_aoTuV_603.cpp)
else()
    message("PACKED_CODEBOOKS_AOTUV not set or set as off. Using data from packed_codebooks.bin.")
    list(APPEND SOURCE src/ww2ogg/packed_codebooks.cpp)
endif()

# Create library target
package_add_library(WwiseAudioTools ${SOURCE})

target_compile_definitions(WwiseAudioTools PRIVATE KS_STR_ENCODING_NONE)

target_compile_features(WwiseAudioTools PUBLIC cxx_std_23)

target_link_libraries(WwiseAudioTools
    PRIVATE
        kaitai_struct_cpp_stl_runtime::kaitai_struct_cpp_stl_runtime
        Ogg::ogg
        Vorbis::vorbis
)

# CLI
if(BUILD_CLI)
    package_add_executable(WwiseAudioTools_CLI cli/main.cpp)

    target_link_libraries(WwiseAudioTools_CLI WwiseAudioTools::WwiseAudioTools)

    set_target_properties(WwiseAudioTools_CLI PROPERTIES
        OUTPUT_NAME wwtools
    )
endif()

# Install the package
package_install()

# Add tests
add_executable(tests tests/wem.cpp)
target_compile_definitions(tests PRIVATE KS_STR_ENCODING_NONE)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain WwiseAudioTools::WwiseAudioTools)

# Copy test data to test location
add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata
    $<TARGET_FILE_DIR:tests>/testdata)
