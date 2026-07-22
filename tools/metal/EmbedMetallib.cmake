# Embeds a compiled Metal library as a byte array header.
# Arguments: INPUT (metallib file), OUTPUT (header file)

file(READ ${INPUT} content HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes ${content})
file(WRITE ${OUTPUT} "// Generated from DefaultShader.metal\n\nconstexpr unsigned char defaultShaderLibrary[] = {${bytes}};\n")
