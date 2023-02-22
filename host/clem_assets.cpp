#include "clem_assets.hpp"

#include "stb_image.h"

#include "sokol/sokol_gfx.h"

namespace ClemensHostAssets {

#include "images/card_png.h"
#include "images/debugger_png.h"
#include "images/fast_emulate_png.h"
#include "images/help_png.h"
#include "images/joystick_png.h"
#include "images/load_png.h"
#include "images/play_run_png.h"
#include "images/power_png.h"
#include "images/reboot_png.h"
#include "images/save_png.h"
#include "images/settings_png.h"
#include "images/stop_run_png.h"

namespace {
struct ImageInfo {
    int width;
    int height;
    sg_image image;
};
} // namespace

static ImageInfo g_allImages[kImageCount];

ImageInfo loadImageFromPNG(const uint8_t *data, size_t dataSize) {
    ImageInfo image{};

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
        image.image = sg_make_image(imageDesc);
        stbi_image_free(pixels);
        image.width = width;
        image.height = height;
    }

    return image;
}

void initialize() {
    //  TODO: might make sense to make this a sprite sheet...
    g_allImages[kPowerButton] = loadImageFromPNG(power_png, power_png_len);
    g_allImages[kPowerCycle] = loadImageFromPNG(reboot_png, reboot_png_len);
    g_allImages[kJoystick] = loadImageFromPNG(joystick_png, joystick_png_len);
    g_allImages[kLoad] = loadImageFromPNG(load_png, load_png_len);
    g_allImages[kSave] = loadImageFromPNG(save_png, save_png_len);
    g_allImages[kRunMachine] = loadImageFromPNG(play_run_png, play_run_png_len);
    g_allImages[kStopMachine] = loadImageFromPNG(stop_run_png, stop_run_png_len);
    g_allImages[kDebugger] = loadImageFromPNG(debugger_png, debugger_png_len);
    g_allImages[kSettings] = loadImageFromPNG(settings_png, settings_png_len);
    g_allImages[kHelp] = loadImageFromPNG(help_png, help_png_len);
    g_allImages[kCard] = loadImageFromPNG(card_icon_png, card_icon_png_len);
    g_allImages[kFastEmulate] = loadImageFromPNG(fast_emulate_png, fast_emulate_png_len);
}

uintptr_t getImage(ImageId imageId) { return g_allImages[imageId].image.id; }

float getImageAspect(ImageId imageId) {
    return float(g_allImages[imageId].width) / g_allImages[imageId].height;
}

void terminate() {
    for (int i = 0; i < kImageCount; ++i) {
        sg_destroy_image(g_allImages[i].image);
        g_allImages[i].image.id = SG_INVALID_ID;
    }
}

} // namespace ClemensHostAssets
