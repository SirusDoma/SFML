# the example mixes SFML with raw Metal calls, its source is Objective-C++
enable_language(OBJCXX)

# all source files
set(SRC Metal.mm)

# define the metal target
sfml_add_example(metal GUI_APP
                 SOURCES ${SRC}
                 DEPENDS SFML::Graphics)

# raw Metal dependencies
target_link_libraries(metal PRIVATE "-framework Metal" "-framework Foundation")
