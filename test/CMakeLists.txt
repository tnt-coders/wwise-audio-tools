find_package(Catch2 REQUIRED)

add_executable(tests wem.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain WwiseAudioTools::WwiseAudioTools)

include(Catch)
catch_discover_tests(tests)

# Copy test data to test location
add_custom_command(
    TARGET tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/testdata
            $<TARGET_FILE_DIR:tests>/testdata)
