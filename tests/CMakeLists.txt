# Each test file becomes its own small executable registered with CTest,
# linked against BytesCore so it exercises the real production code.

set(BYTES_TEST_FILES
    test_classifier.cpp
    test_ruleengine.cpp
    test_duplicatehandler.cpp
    test_database.cpp
    test_markdownlogger.cpp
    test_safemover.cpp
    test_undomanager.cpp
    test_cleaner.cpp
    test_startup.cpp
)

foreach(test_file ${BYTES_TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE BytesCore Qt6::Test)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endforeach()
