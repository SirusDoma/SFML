
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