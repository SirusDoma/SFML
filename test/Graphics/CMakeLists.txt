
set(GRAPHICS_SRC
    BackendParity.test.cpp
    BlendMode.test.cpp
    CircleShape.test.cpp
    Color.test.cpp
    ConvexShape.test.cpp
    CoordinateType.test.cpp
    Drawable.test.cpp
    Font.test.cpp
    Glsl.test.cpp
    Glyph.test.cpp
    Renderer.test.cpp
    Image.test.cpp
    Rect.test.cpp
    RectangleShape.test.cpp
    Render.test.cpp
    RenderStates.test.cpp
    RenderTarget.test.cpp
    RenderTexture.test.cpp
    RenderWindow.test.cpp
    Shader.test.cpp
    Shape.test.cpp
    Sprite.test.cpp
    StencilMode.test.cpp
    Text.test.cpp
    Texture.test.cpp
    Transform.test.cpp
    Transformable.test.cpp
    Vertex.test.cpp
    VertexArray.test.cpp
    VertexBuffer.test.cpp
    View.test.cpp
)
if(SFML_OS_WINDOWS)
    list(APPEND GRAPHICS_SRC
        WindowHandleWin32.test.cpp
    )
endif()
sfml_add_test(test-sfml-graphics "${GRAPHICS_SRC}" SFML::Graphics)
if(SFML_RUN_DISPLAY_TESTS)
    target_compile_definitions(test-sfml-graphics PRIVATE SFML_RUN_DISPLAY_TESTS)
endif()

# Run the same rendering checks on the Direct3D 11 backend
if(SFML_OS_WINDOWS AND SFML_ENABLE_D3D11)
    sfml_add_test(test-sfml-graphics-d3d11 "BackendParity.test.cpp" SFML::Graphics)
    target_compile_definitions(test-sfml-graphics-d3d11 PRIVATE SFML_TEST_BACKEND_D3D11)
    if(SFML_RUN_DISPLAY_TESTS)
        target_compile_definitions(test-sfml-graphics-d3d11 PRIVATE SFML_RUN_DISPLAY_TESTS)
    endif()
endif()

# The build options are not visible to the tests, tell them the Metal backend is compiled in
if(SFML_OS_MACOS AND SFML_ENABLE_METAL)
    target_compile_definitions(test-sfml-graphics PRIVATE SFML_TEST_METAL_AVAILABLE)
endif()

# Run the same rendering checks on the Metal backend
if(SFML_OS_MACOS AND SFML_ENABLE_METAL)
    sfml_add_test(test-sfml-graphics-metal "BackendParity.test.cpp" SFML::Graphics)
    target_compile_definitions(test-sfml-graphics-metal PRIVATE SFML_TEST_BACKEND_METAL)
    if(SFML_RUN_DISPLAY_TESTS)
        target_compile_definitions(test-sfml-graphics-metal PRIVATE SFML_RUN_DISPLAY_TESTS)
    endif()
endif()

# Run the same rendering checks on the Vulkan backend
if((SFML_OS_WINDOWS OR SFML_OS_LINUX) AND SFML_ENABLE_VULKAN)
    sfml_add_test(test-sfml-graphics-vulkan "BackendParity.test.cpp" SFML::Graphics)
    target_compile_definitions(test-sfml-graphics-vulkan PRIVATE SFML_TEST_BACKEND_VULKAN)
    if(SFML_RUN_DISPLAY_TESTS)
        target_compile_definitions(test-sfml-graphics-vulkan PRIVATE SFML_RUN_DISPLAY_TESTS)
    endif()

    # Without the runtime shader compiler the parity shaders are consumed as
    # SPIR-V, compiled at build time the same way applications compile theirs
    find_program(SFML_VULKAN_TEST_DXC_COMPILER dxc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
    set(SFML_VULKAN_TEST_DXC ${SFML_VULKAN_TEST_DXC_COMPILER})
    if(SFML_VULKAN_TEST_DXC)
        execute_process(COMMAND ${SFML_VULKAN_TEST_DXC} -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/FlatFragment.hlsl -Fo ${CMAKE_CURRENT_BINARY_DIR}/VulkanDxcProbe.spv
                        RESULT_VARIABLE SFML_VULKAN_TEST_DXC_PROBE
                        OUTPUT_QUIET ERROR_QUIET)
        if(NOT SFML_VULKAN_TEST_DXC_PROBE EQUAL 0)
            set(SFML_VULKAN_TEST_DXC "")
        endif()
    endif()
    if(SFML_VULKAN_TEST_DXC)
        set(SFML_VULKAN_TEST_SHADER_HEADERS "")
        foreach(SHADER IN ITEMS TintFragment:ps_6_0 MatricesVertex:vs_6_0 FlatFragment:ps_6_0 SampleFragment:ps_6_0)
            string(REPLACE ":" ";" SHADER "${SHADER}")
            list(GET SHADER 0 SHADER_NAME)
            list(GET SHADER 1 SHADER_PROFILE)
            add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Vulkan${SHADER_NAME}Shader.hpp
                               COMMAND ${SFML_VULKAN_TEST_DXC} -spirv -fspv-target-env=vulkan1.2 -T ${SHADER_PROFILE} -E main ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/${SHADER_NAME}.hlsl -Fo ${CMAKE_CURRENT_BINARY_DIR}/Vulkan${SHADER_NAME}Shader.spv
                               COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/Vulkan${SHADER_NAME}Shader.spv -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/Vulkan${SHADER_NAME}Shader.hpp -DVARIABLE=vulkan${SHADER_NAME}ShaderSpirv -P ${PROJECT_SOURCE_DIR}/tools/vulkan/EmbedSpirv.cmake
                               DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/${SHADER_NAME}.hlsl ${PROJECT_SOURCE_DIR}/tools/vulkan/EmbedSpirv.cmake
                               COMMENT "Compiling the Vulkan parity shader ${SHADER_NAME}")
            list(APPEND SFML_VULKAN_TEST_SHADER_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/Vulkan${SHADER_NAME}Shader.hpp)
        endforeach()
        target_sources(test-sfml-graphics-vulkan PRIVATE ${SFML_VULKAN_TEST_SHADER_HEADERS})
        target_include_directories(test-sfml-graphics-vulkan PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
        target_compile_definitions(test-sfml-graphics-vulkan PRIVATE SFML_TEST_VULKAN_SPIRV_SHADERS)
    endif()
endif()

if (XCODE AND SFML_OS_IOS)
    set(GRAPHICS_RESOURCES
        invalid_shader.vert
        sfml-logo-big.bmp
        sfml-logo-big.gif
        sfml-logo-big.jpg
        sfml-logo-big.png
        sfml-logo-big.psd
        shader-🐌.vert
        shader-ń.vert
        shader.frag
        shader.geom
        shader.vert
        tuffy-🐌.ttf
        tuffy-ń.ttf
        tuffy.ttf
    )
    target_sources(test-sfml-graphics PRIVATE ${GRAPHICS_RESOURCES})
    set_source_files_properties(${GRAPHICS_RESOURCES} MACOSX_PACKAGE_LOCATION "Graphics")
endif()