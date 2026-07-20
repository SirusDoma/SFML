# all source files
set(SRC Direct3D.cpp)

# define the direct3d target
sfml_add_example(direct3d GUI_APP
                 SOURCES ${SRC}
                 DEPENDS SFML::Graphics)

# raw Direct3D dependencies
target_link_libraries(direct3d PRIVATE d3d11 d3dcompiler)
