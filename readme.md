[![SFML logo](https://www.sfml-dev.org/images/logo.png)](https://www.sfml-dev.org)

# SFML — Simple and Fast Multimedia Library

SFML is a simple, fast, cross-platform and object-oriented multimedia API. It provides access to windowing, graphics, audio and network. It is written in C++ and has bindings for various languages such as C, .Net, Ruby, Python.

## About This Fork

This fork makes the rendering pipeline of the graphics module modular. The pipeline is split from its OpenGL implementation and placed behind a set of internal backend interfaces, and three additional renderers are implemented on top of them: Direct3D 11 on Windows, Metal on macOS, and Vulkan on Windows and Linux. OpenGL remains the default renderer on every platform; another renderer can be selected at runtime with `sf::setRenderer` before the first graphics resource is created. The Vulkan renderer requires a driver supporting Vulkan 1.2 and is only listed by `sf::getAvailableRenderers` on systems that have one; it is programmed in the same HLSL dialect as the Direct3D 11 renderer, consumed as SPIR-V bytecode compiled offline with `dxc`, or as HLSL source text when SFML is built with `SFML_VULKAN_RUNTIME_SHADER_COMPILER` enabled.

Although the additional renderers are built on this abstraction, supporting user-provided/hot-swappable rendering backends is not the goal. The goal is to make the existing pipeline customizable: for example, a custom render target can now be written without dealing with the rendering implementation directly, where previously `sf::RenderTarget` was tightly coupled to OpenGL states. This is useful when you have to deal with vertices but in high-level context, such as implementing sprite batching.

The changes to the public API are kept non-intrusive, so existing projects can use this fork without any modification. In turn, the semantics change in some places behind the scenes. For example, the Direct3D 11 and Metal renderers defer and batch draws internally to accommodate the immediate drawing style of the SFML API, since issuing every draw immediately through these APIs would hurt performance. The OpenGL renderer is unchanged in this regard: calling `draw` there still issues exactly one draw call immediately, as it always has. Where the difference would be observable, the original semantics are maintained instead. For example, clearing with a scissored view only clears the scissor rectangle on every renderer, like it does on OpenGL.

Mixing SFML rendering with raw graphics API calls remains supported on every renderer. Just like the original SFML allows raw OpenGL calls alongside its own drawing, the `sf::OpenGL`, `sf::D3D11`, `sf::Metal` and `sf::Vulkan` namespaces expose the underlying device and resources of the active renderer, so raw Direct3D 11, Metal and Vulkan code can be mixed into an SFML frame the same way raw OpenGL always could. The `opengl`, `direct3d`, `metal` and `vulkan` examples demonstrate this. (`sf::Vulkan` spans two headers: the window module part for writing your own Vulkan renderer on top of `sf::Window`, and the graphics module part in `SFML/Graphics/VulkanInterop.hpp` for interoperating with the Vulkan renderer of the graphics module.)

## State of Development

Development of this fork takes place in the `graphics/modular` branch.

## External libraries used by SFML

-   [_stb_image_ and _stb_image_write_](https://github.com/nothings/stb) are [public domain](https://github.com/nothings/stb/blob/master/LICENSE)
-   [_freetype_](https://gitlab.freedesktop.org/freetype/freetype) is under the [FreeType license or the GPL license](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/LICENSE.TXT)
-   [_libogg_](https://gitlab.xiph.org/xiph/ogg) is under the [BSD license](https://gitlab.xiph.org/xiph/ogg/-/blob/master/COPYING)
-   [_libvorbis_](https://gitlab.xiph.org/xiph/vorbis) is under the [BSD license](https://gitlab.xiph.org/xiph/vorbis/-/blob/master/COPYING)
-   [_libflac_](https://gitlab.xiph.org/xiph/flac) is under the [BSD license](https://gitlab.xiph.org/xiph/flac/-/blob/master/COPYING.Xiph)
-   [_dr\_mp3_](https://github.com/mackron/dr_libs) is [public domain or under the MIT No Attribution license](https://github.com/mackron/dr_libs/blob/master/LICENSE)
-   [_miniaudio_](https://github.com/mackron/miniaudio) is [public domain or under the MIT No Attribution license](https://github.com/mackron/miniaudio/blob/master/LICENSE)
-   [_cpp-unicodelib_](https://github.com/yhirose/cpp-unicodelib) is under the [MIT license](https://github.com/yhirose/cpp-unicodelib/blob/master/LICENSE)
-   [_HarfBuzz_](https://github.com/harfbuzz/harfbuzz) is under the [Old MIT license](https://github.com/harfbuzz/harfbuzz/blob/main/COPYING)
-   [_SheenBidi_](https://github.com/Tehreer/SheenBidi) is under the [Apache license](https://github.com/Tehreer/SheenBidi/blob/master/LICENSE)
-   [_qoi_](https://github.com/phoboslab/qoi) is under the [MIT license](https://github.com/phoboslab/qoi/blob/master/LICENSE)
-   [_Mbed TLS_](https://github.com/Mbed-TLS/mbedtls) is under the [Apache license or the GPL license](https://github.com/Mbed-TLS/mbedtls/blob/main/LICENSE)
-   [_wepoll_](https://github.com/piscisaureus/wepoll) is under the [BSD license](https://github.com/piscisaureus/wepoll/blob/dist/LICENSE)
-   [_libssh2_](https://github.com/libssh2/libssh2) is under the [BSD license](https://github.com/libssh2/libssh2/blob/master/COPYING)
