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

# Create library target - CMake automatically uses BUILD_SHARED_LIBS
package_add_library(WwiseAudioTools ${SOURCE})

# Create executable target
package_add_executable(WwiseAudioTools_CLI ${SOURCE} src/main.cpp)

# Add compile definitions
target_compile_definitions(WwiseAudioTools PRIVATE KS_STR_ENCODING_NONE)
target_compile_definitions(WwiseAudioTools_CLI PRIVATE KS_STR_ENCODING_NONE)

# Link dependencies - these will be propagated to consuming targets
target_link_libraries(WwiseAudioTools PUBLIC Ogg::ogg Vorbis::vorbis)

# Executable outputs as 'wwtools' (the CLI tool keeps the original name)
set_target_properties(WwiseAudioTools_CLI PROPERTIES OUTPUT_NAME wwtools)

# Standard filesystem library needs additional linking options on older compilers
# https://en.cppreference.com/w/cpp/filesystem#Notes
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
    target_link_libraries(WwiseAudioTools_CLI Ogg::ogg Vorbis::vorbis stdc++fs)
  else()
    target_link_libraries(WwiseAudioTools_CLI Ogg::ogg Vorbis::vorbis)
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(WwiseAudioTools_CLI Ogg::ogg Vorbis::vorbis c++fs)
  else()
    target_link_libraries(WwiseAudioTools_CLI Ogg::ogg Vorbis::vorbis)
  endif()
else()
  target_link_libraries(WwiseAudioTools_CLI Ogg::ogg Vorbis::vorbis)
endif()

# Install the package
package_install()

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND AND DOWNLOAD_CATCH2)
  include(FetchContent)

  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.12.0
  )
  
  message("Catch2 v3 not found locally. Fetching it from the git repository...")
  FetchContent_MakeAvailable(catch2)
  set(FETCHCONTENT_UPDATES_DISCONNECTED_CATCH2 ON) # don't update

  set(Catch2_FOUND TRUE)
endif()

if(Catch2_FOUND)
  add_executable(tests ${SOURCE} tests/wem.cpp)
  target_include_directories(tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/libogg/include
      ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/include
      ${CMAKE_CURRENT_BINARY_DIR}/libogg/include
  )
  target_compile_definitions(tests PRIVATE KS_STR_ENCODING_NONE)
  target_link_libraries(tests PRIVATE Catch2::Catch2WithMain Ogg::ogg vorbis::vorbis)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else()
  message("Catch2 v3 was not found and not dowloaded. Testing target won't be available.")
endif()
