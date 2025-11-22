set(BIN_NAME dpdk_forwarder_tests)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA REQUIRED cmocka)

# Test source files
set(TEST_SOURCES
    main.c
    limiter_tests.c
)

# Create test executable
add_executable(${BIN_NAME} ${TEST_SOURCES})

target_include_directories(${BIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMOCKA_INCLUDE_DIRS}
)

target_link_libraries(${BIN_NAME}
    ${CMOCKA_LIBRARIES}
)

# Register tests with CTest
add_test(NAME limiter_tests COMMAND ${BIN_NAME})
set_tests_properties(limiter_tests PROPERTIES
    ENVIRONMENT "CMOCKA_MESSAGE_OUTPUT=stdout"
)
