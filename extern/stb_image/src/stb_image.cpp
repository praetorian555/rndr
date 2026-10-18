// Only the decoders rndr reads: PNG, JPEG, BMP, TGA and Radiance HDR.
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PSD
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image/stb_image_resize2.h"
