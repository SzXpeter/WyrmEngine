function (add_slang_shader_target TARGET)
    cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})

    set(SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(ENTRY_POINTS -entry vertMain -entry fragMain)

    add_custom_command(
            OUTPUT ${SHADERS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )

    add_custom_command(
            OUTPUT  ${SHADERS_DIR}/shader.spv
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o shader.spv
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
    )

    add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/shader.spv)
endfunction()

add_slang_shader_target(foo SOURCES "shader.slang")
set_source_files_properties(foo PROPERTIES GENERATED TRUE)
add_dependencies(WyrmEngine foo)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shader.spv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")