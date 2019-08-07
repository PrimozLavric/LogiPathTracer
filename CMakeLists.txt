cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

# Metadata
project(LogiPathTracer VERSION 0.1.0 LANGUAGES CXX)

include(${PROJECT_SOURCE_DIR}/cmake_modules/CompileShaders.cmake)

# Add path to CMake custom modules.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

# C++ Google Test
option(BUILD_TESTS "Build tests." OFF)
option(BUILD_DOC "Build documentation" OFF)
option(BUILD_EXAMPLES "Build examples" ON)

##############################################
# BUILD LOGI PATH TRACER
############################################## 

# Libraries path
include(ExternalDependencies.cmake)

file(GLOB_RECURSE SRC_LIST "src/*.cpp")

add_executable(logi_path_tracer ${SRC_LIST})

target_include_directories(logi_path_tracer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        )

target_link_libraries(logi_path_tracer logi LogiSceneGraph CppGLFW)

# Copy resources directory.
add_custom_command(TARGET logi_path_tracer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
        )

compile_shaders(logi_path_tracer shaders)

##########################################################
####################### DOXYGEN ##########################
##########################################################

if (BUILD_DOC)
    # Check if Doxygen is installed
    find_package(Doxygen)

    if (DOXYGEN_FOUND)
        # Set input and output files
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

        # Request to configure the file
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        message("Doxygen build started")

        # Note the option ALL which allows to build the docs together with the application
        add_custom_target(doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM)
    else (DOXYGEN_FOUND)
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif (BUILD_DOC)

##############################################
# TESTS
##############################################

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif (BUILD_TESTS)