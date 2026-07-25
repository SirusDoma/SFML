# all source files
set(SRC VulkanInterop.cpp)

# define the vulkan target
sfml_add_example(vulkan GUI_APP
                 SOURCES ${SRC}
                 DEPENDS SFML::Graphics)

# reuse the GLAD Vulkan loader header of the window module's vulkan_window example
target_include_directories(vulkan SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/examples/vulkan_window)
