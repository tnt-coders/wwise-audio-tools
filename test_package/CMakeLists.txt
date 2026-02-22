cmake_minimum_required(VERSION 3.24)
project(PackageTest CXX)

find_package(WwiseAudioTools REQUIRED CONFIG)

add_executable(test_package test_package.cpp)
target_link_libraries(test_package WwiseAudioTools::WwiseAudioTools)
