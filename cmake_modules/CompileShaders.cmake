macro(compile_shaders target_name shaders_path)


    file(GLOB_RECURSE GLSL_SOURCE_FILES
            "${shaders_path}/*.frag"
            "${shaders_path}/*.vert"
            "${shaders_path}/*.comp"
            "${shaders_path}/*.rchit"
            "${shaders_path}/*.rmiss"
            "${shaders_path}/*.rgen"
            )

    foreach (GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(GLSL_SUBDIR ${GLSL} DIRECTORY)
        file(RELATIVE_PATH GLSL_SUBDIR ${CMAKE_CURRENT_SOURCE_DIR}/${shaders_path} ${GLSL_SUBDIR})

        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/${shaders_path}/${GLSL_SUBDIR}/${FILE_NAME}.spv")
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${shaders_path}/${GLSL_SUBDIR}/"
                COMMAND glslangValidator -V -Od ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach (GLSL)

    add_custom_target(${target_name}_shaders DEPENDS ${SPIRV_BINARY_FILES})

    add_dependencies(${target_name} ${target_name}_shaders)

    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/${shaders_path}/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/${shaders_path}"
            "$<TARGET_FILE_DIR:${target_name}>/${shaders_path}"
            )

endmacro()