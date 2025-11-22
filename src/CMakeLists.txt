set(BIN_NAME dpdk_forwarder)

# Find DPDK package
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Source files
set(SOURCES
    main.c
    cli.c
    app.c
    simple_mode.c
    multicore_mode.c
    signals.c
    statistics.c
)

# Create the app binary
add_executable(${BIN_NAME} ${SOURCES})

target_include_directories(${BIN_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${DPDK_INCLUDE_DIRS}
)
target_compile_options(${BIN_NAME}
    PRIVATE
    ${DPDK_CFLAGS_OTHER}
)
target_link_libraries(${BIN_NAME}
    ${DPDK_LIBRARIES}
)
target_link_directories(${BIN_NAME}
    PRIVATE
    ${DPDK_LIBRARY_DIRS}
)

