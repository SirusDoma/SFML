# Embeds a compiled SPIR-V module as a 32-bit word array header, the
# alignment Vulkan requires for shader module creation.
# Arguments: INPUT (spv file), OUTPUT (header file), VARIABLE (array name)

file(READ ${INPUT} content HEX)

# SPIR-V is a stream of little-endian 32-bit words: reassemble each
# word from its four bytes
string(REGEX REPLACE "([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])" "0x\\4\\3\\2\\1," words ${content})

file(WRITE ${OUTPUT} "// Generated from DefaultShader.hlsl\n\n#include <cstdint>\n\nconstexpr std::uint32_t ${VARIABLE}[] = {${words}};\n")
