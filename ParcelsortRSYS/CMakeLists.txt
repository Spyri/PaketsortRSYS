cmake_minimum_required(VERSION 3.5.1)

# Project name
project(rtai_kernel_module)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set the kernel source directory
set(KERNEL_SRC /lib/modules/${CMAKE_SYSTEM_VERSION}/build)

# Set the RTAI base directory
set(RTAI_BASE /usr/realtime)

# Add the kernel module source file
set(MODULE_SOURCES test.c)

# Add custom target to build the kernel module
add_custom_target(${PROJECT_NAME} ALL
    COMMAND ${CMAKE_COMMAND} -E env "KERNEL_SRC=${KERNEL_SRC}" "RTAI_BASE=${RTAI_BASE}"
    ${CMAKE_MAKE_PROGRAM} -C ${KERNEL_SRC} M=${CMAKE_SOURCE_DIR} modules
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building RTAI kernel module"
)

# Clean target to clean the kernel module build
add_custom_target(clean-${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E env "KERNEL_SRC=${KERNEL_SRC}" "RTAI_BASE=${RTAI_BASE}"
    ${CMAKE_MAKE_PROGRAM} -C ${KERNEL_SRC} M=${CMAKE_SOURCE_DIR} clean
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning RTAI kernel module build"
)
