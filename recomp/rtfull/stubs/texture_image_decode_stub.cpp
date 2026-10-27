// Replaces runtime/src/texture_image_decode.cpp (needs libwebp, not built here): HD texture packs are off in this build.
#include "texture_image_decode.h"

bool texture_image_probe(const uint8_t*, size_t, size_t, TextureImageProbe&, std::string& error) {
    error = "texture image decoding is not built in (psxrt stub)";
    return false;
}
bool texture_image_decode(const uint8_t*, size_t, size_t, DecodedTextureImage&, std::string& error) {
    error = "texture image decoding is not built in (psxrt stub)";
    return false;
}
