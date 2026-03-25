# all source files
set(SRC CustomRenderTarget.cpp)

# define the custom_render_target target
sfml_add_example(custom_render_target GUI_APP
                 SOURCES ${SRC}
                 DEPENDS SFML::Graphics)
