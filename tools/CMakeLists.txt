cmake_minimum_required(VERSION 2.9)
set(TARGET_NAME "crono_pci_drvmod" CACHE STRING "Project Target Name")
project(${TARGET_NAME})

# Install project tools, then include `crono_common.cmake` and initialize it
IF(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/crono_common.cmake") 
        message(STATUS "Crono: Installing Cronologic Project Tools ...")
        execute_process(COMMAND "conan" install crono_project_tools/[~1.1.0]@ 
                -o support_x64_32=False         # Support x64_32
                -if ${CMAKE_CURRENT_SOURCE_DIR} # `tools` folder
)
ENDIF()

# `crono_common.cmake` initialization and configuration
set(CRONO_TARGET_NAME ${TARGET_NAME})
set(CRONO_PROJECT_TYPE EXECUTABLE)
include(${CMAKE_CURRENT_SOURCE_DIR}/crono_common.cmake)
crono_cmake_common_init()
string(TOLOWER ${CRONO_BUILD_TYPE} CRONO_CONFIG_DIR)

# Set kernel build path, and validate it
set(kerneldir "" CACHE STRING "Path to the kernel build directory")
if("${kerneldir}" STREQUAL "")
    execute_process(COMMAND uname -r 
        OUTPUT_VARIABLE uname_r
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(kerneldir "/lib/modules/${uname_r}/build")
endif()
IF(EXISTS ${kerneldir})
    message(STATUS "Crono: building from kernel directory <${kerneldir}>")
ELSE()
    message(FATAL_ERROR "Crono: kernel module directory <${kerneldir}> is not found"
                        ", please install it.")
ENDIF()    

# Set directories
set(CRONO_SRC_DIR "${PROJECT_SOURCE_DIR}/../src" 
        CACHE STRING "Path to the project source directory `/src`")
message(STATUS "Crono: source directory is <${CRONO_SRC_DIR}>")

# Copy source code
execute_process(
    COMMAND "echo" Crono: copying source files to binary directory 
    # Copy c files from source to build directory
    COMMAND sh -c "cp ${CRONO_SRC_DIR}/*.c ${CMAKE_CURRENT_BINARY_DIR}"
    # Copy headers from source to build directory
    COMMAND sh -c "cp ${CRONO_SRC_DIR}/*.h ${CMAKE_CURRENT_BINARY_DIR}"
)

# Set sources for build
set(CRONO_SRCS "crono_kernel_module.c" CACHE STRING "Source Files Names")

# Set Kbuild parameters - CRONO_TARGET_OBJS
set(CRONO_TARGET_OBJS "crono_kernel_module.o" CACHE STRING "Object Files Names")

# Set Kbuild parameters - CRONO_CCFLAGS
set(CRONO_CCFLAGS   "-DCRONO_KERNEL_MODE -m64 " CACHE STRING "CC Flags")
string(PREPEND CRONO_CCFLAGS " -I${PROJECT_SOURCE_DIR}/../include " )

# Set Kbuild parameters - Configuration-specific flags
if ("${CRONO_CONFIG_DIR}" STREQUAL "debug")
    string(PREPEND CRONO_CCFLAGS " -DCRONO_DEBUG_ENABLED -DDEBUG=1 ")
endif()

# Set Kbuild parameters - Preprocessor directive for old Kernel Version 
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${uname_r})
if(LINUX_KERNEL_VERSION VERSION_GREATER_EQUAL 5.6)
    message(STATUS  "Crono: kernel version is <${LINUX_KERNEL_VERSION}>")
else()
    string(PREPEND CRONO_CCFLAGS " -DOLD_KERNEL_FOR_PIN ")
    message(STATUS  "Crono: kernel version is <${LINUX_KERNEL_VERSION}> less than 5.6"
                    ", Supporting old versions")
endif()

# Copy and configure `Kbuild`
execute_process(
    COMMAND sh -c "cp ${PROJECT_SOURCE_DIR}/Kbuild.cmake.in ${CMAKE_CURRENT_BINARY_DIR}")
CONFIGURE_FILE(${CMAKE_CURRENT_BINARY_DIR}/Kbuild.cmake.in ${CMAKE_BINARY_DIR}/Kbuild)

# Set the output directory ____________________________________________
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY 
        ${CMAKE_BINARY_DIR}/build/linux/x86_64/${CRONO_CONFIG_DIR}/bin)

# Create target, and copy it to `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 
add_custom_target(${CRONO_TARGET_NAME} ALL
    COMMAND "echo" Crono: building driver module ${CRONO_TARGET_NAME}  
    COMMAND make -j2 -C ${kerneldir} M=${CMAKE_BINARY_DIR} 
    COMMAND mkdir -p ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/*.ko ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    COMMAND echo "Crono: output directory is <${CMAKE_RUNTIME_OUTPUT_DIRECTORY}>"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building crono_pci_drvmod.ko"
    SOURCES ${CRONO_SRCS}
    VERBATIM
)

# Publish packages on conan local cache _______________________________ 
IF (NOT CRONO_PUBLISH_LOCAL_PKG STREQUAL "N")
    crono_target_post_build_export_pkg_main()
    crono_target_post_build_export_pkg("${CMAKE_CURRENT_SOURCE_DIR}/bin_pkg")
    crono_target_post_build_export_pkg("${CMAKE_CURRENT_SOURCE_DIR}/headers_pkg")
ENDIF()
