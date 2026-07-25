# all source files
set(SRC Vulkan.cpp)

# define the vulkan_window target
sfml_add_example(vulkan_window GUI_APP
                 SOURCES ${SRC}
                 DEPENDS SFML::Graphics)

# external dependency headers
target_include_directories(vulkan_window SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/examples/vulkan_window)
