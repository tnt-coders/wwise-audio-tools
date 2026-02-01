cmake_minimum_required(VERSION 3.12)

# Required for MSVC to cooperate with Conan 2.X+
cmake_policy(SET CMP0091 NEW)

project(wwise-audio-tools VERSION 1.0.0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)

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
    src/kaitai/structs/wem.cpp
  )

option(PACKED_CODEBOOKS_AOTUV "Use data from packed_codebooks_aoTuV_603.bin instead of regular packed_codebooks.bin" ON)

if(PACKED_CODEBOOKS_AOTUV)
  message("PACKED_CODEBOOKS_AOTUV set as on. Using data from packed_codebooks_aoTuV_603.bin.")
  list(APPEND SOURCE src/ww2ogg/packed_codebooks_aoTuV_603.cpp)
else()
  message("PACKED_CODEBOOKS_AOTUV not set or set as off. Using data from packed_codebooks.bin.")
  list(APPEND SOURCE src/ww2ogg/packed_codebooks.cpp)
endif()

# Dependencies are now built from submodules (see above)
# If you want to use system/Conan packages instead, comment out the submodule
# section above and uncomment these lines:
# find_package(Ogg REQUIRED)
# find_package(Vorbis REQUIRED)

# Create library targets
add_library(wwtools-shared-lib SHARED ${SOURCE})
add_library(wwtools-static-lib STATIC ${SOURCE})
add_executable(wwtools-bin ${SOURCE} src/main.cpp)

# Add alias targets for easier integration
add_library(wwtools::shared ALIAS wwtools-shared-lib)
add_library(wwtools::static ALIAS wwtools-static-lib)

# Configure include directories for library targets
target_include_directories(wwtools-shared-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libogg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/include
)

target_include_directories(wwtools-static-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libogg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/include
)

target_include_directories(wwtools-bin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libogg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/include
)

# Add compile definitions
target_compile_definitions(wwtools-shared-lib PRIVATE KS_STR_ENCODING_NONE)
target_compile_definitions(wwtools-static-lib PRIVATE KS_STR_ENCODING_NONE)
target_compile_definitions(wwtools-bin PRIVATE KS_STR_ENCODING_NONE)

# Link dependencies - these will be propagated to consuming targets
target_link_libraries(wwtools-shared-lib PUBLIC Ogg::ogg vorbis::vorbis)
target_link_libraries(wwtools-static-lib PUBLIC Ogg::ogg vorbis::vorbis)

set_target_properties(wwtools-bin PROPERTIES OUTPUT_NAME wwtools)
set_target_properties(wwtools-shared-lib PROPERTIES OUTPUT_NAME wwtools)
set_target_properties(wwtools-static-lib PROPERTIES OUTPUT_NAME wwtools)

# Standard filesystem library needs additional linking options on older compilers
# https://en.cppreference.com/w/cpp/filesystem#Notes
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
    target_link_libraries(wwtools-bin Ogg::ogg vorbis::vorbis stdc++fs)
  else()
    target_link_libraries(wwtools-bin Ogg::ogg vorbis::vorbis)
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(wwtools-bin Ogg::ogg vorbis::vorbis c++fs)
  else()
    target_link_libraries(wwtools-bin Ogg::ogg vorbis::vorbis)
  endif()
else()
  target_link_libraries(wwtools-bin Ogg::ogg vorbis::vorbis)
endif()

# Installation rules
install(TARGETS wwtools-bin
    RUNTIME DESTINATION bin
)

# Install libraries with export for find_package() support
install(TARGETS wwtools-static-lib wwtools-shared-lib
    EXPORT wwtoolsTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

# Install the export set for find_package()
install(EXPORT wwtoolsTargets
    FILE wwtoolsTargets.cmake
    NAMESPACE wwtools::
    DESTINATION lib/cmake/wwtools
)

# Generate and install package configuration files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/wwtoolsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wwtoolsConfig.cmake
    INSTALL_DESTINATION lib/cmake/wwtools
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wwtoolsConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/wwtoolsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/wwtoolsConfigVersion.cmake
    DESTINATION lib/cmake/wwtools
)

option(DOWNLOAD_CATCH2 "Download Catch2 framework if it won't be found locally" OFF)

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
