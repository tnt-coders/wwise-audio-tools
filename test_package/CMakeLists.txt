cmake_minimum_required(VERSION 3.24)
project(PackageTest CXX)

# Conan profiles can inject CMAKE_CXX_STANDARD (overriding our settings). Clear it so transitive
# usage requirements from WwiseAudioTools decide the standard.
unset(CMAKE_CXX_STANDARD CACHE)
unset(CMAKE_CXX_EXTENSIONS CACHE)

find_package(WwiseAudioTools REQUIRED CONFIG)

add_executable(test_package test_package.cpp)
target_link_libraries(test_package WwiseAudioTools::WwiseAudioTools)
