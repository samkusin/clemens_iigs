#include "clem_assets.hpp"

#include "stb_image.h"

namespace ClemensHostAssets {

#include "images/card_png.h"
#include "images/joystick_png.h"
#include "images/load_png.h"
#include "images/power_png.h"
#include "images/reboot_png.h"
#include "images/save_png.h"

static sg_image g_allImages[kImageCount];

sg_image loadImageFromPNG(const uint8_t *data, size_t dataSize) {
    sg_image image{};
    int width, height, ncomp;
    uint8_t *pixels = stbi_load_from_memory(data, dataSize, &width, &height, &ncomp, 4);
    if (pixels) {
        sg_image_desc imageDesc{};
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
        imageDesc.min_filter = SG_FILTER_LINEAR;
        imageDesc.mag_filter = SG_FILTER_LINEAR;
        imageDesc.data.subimage[0][0].ptr = pixels;
        imageDesc.data.subimage[0][0].size = width * height * 4;
        image = sg_make_image(imageDesc);
        stbi_image_free(pixels);
    }
    return image;
}

void initialize() {
    g_allImages[kPowerButton] = loadImageFromPNG(power_png, power_png_len);
    g_allImages[kPowerCycle] = loadImageFromPNG(reboot_png, reboot_png_len);
    g_allImages[kJoystick] = loadImageFromPNG(joystick_png, joystick_png_len);
    g_allImages[kLoad] = loadImageFromPNG(load_png, load_png_len);
    g_allImages[kSave] = loadImageFromPNG(save_png, save_png_len);
    g_allImages[kCard] = loadImageFromPNG(card_icon_png, card_icon_png_len);
}

sg_image getImage(ImageId imageId) { return g_allImages[imageId]; }

void terminate() {
    for (int i = 0; i < kImageCount; ++i) {
        sg_destroy_image(g_allImages[i]);
        g_allImages[i].id = SG_INVALID_ID;
    }
}

} // namespace ClemensHostAssets
