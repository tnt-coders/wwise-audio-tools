cmake_minimum_required(VERSION 3.24)

# Required for MSVC to cooperate with Conan 2.X+
cmake_policy(SET CMP0091 NEW)

project(WwiseAudioTools VERSION 1.0.0 LANGUAGES C CXX)

option(PACKED_CODEBOOKS_AOTUV "Use data from packed_codebooks_aoTuV_603.bin instead of regular packed_codebooks.bin" ON)
option(DOWNLOAD_CATCH2 "Download Catch2 framework if it won't be found locally" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PackageBuilder REQUIRED)
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
    src/wwtools/wwtools.cpp
    src/wwtools/w3sc.cpp
    src/wwtools/bnk.cpp
    src/kaitai/kaitaistream.cpp
    src/kaitai/structs/bnk.cpp
    src/kaitai/structs/vlq.cpp
    src/kaitai/structs/w3sc.cpp
    src/kaitai/structs/wem.cpp)

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
target_link_libraries(WwiseAudioTools PUBLIC Ogg::ogg Vorbis::vorbis)

# Create executable target
package_add_executable(WwiseAudioTools_CLI src/main.cpp)
target_compile_definitions(WwiseAudioTools_CLI PRIVATE KS_STR_ENCODING_NONE)
target_link_libraries(WwiseAudioTools_CLI WwiseAudioTools::WwiseAudioTools)
set_target_properties(WwiseAudioTools_CLI PROPERTIES OUTPUT_NAME wwtools)

# Install the package
package_install()

find_package(Catch2 QUIET)
if(Catch2_FOUND)
  add_executable(tests tests/wem.cpp)
  target_compile_definitions(tests PRIVATE KS_STR_ENCODING_NONE)
  target_link_libraries(tests PRIVATE Catch2::Catch2WithMain WwiseAudioTools::WwiseAudioTools)

  add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata
    $<TARGET_FILE_DIR:tests>/testdata)
else()
  message("Catch2 v3 was not found and not dowloaded. Testing target won't be available.")
endif()
