#include "clem_assets.hpp"

#include "stb_image.h"

#include "sokol/sokol_gfx.h"
#include <cstring>

namespace ClemensHostAssets {

#include "images/card_png.h"
#include "images/debugger_png.h"
#include "images/disk35_png.h"
#include "images/disk525_png.h"
#include "images/eject_png.h"
#include "images/fast_emulate_png.h"
#include "images/folder_solid_png.h"
#include "images/hdd_png.h"
#include "images/help_png.h"
#include "images/joystick_png.h"
#include "images/load_png.h"
#include "images/lock_png.h"
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

static const char *g_namedImages[kLastNamedImage - kFirstNamedImage] = {
    "",
    NULL,
};

//  this can be freed with freeLoadedBitmap();
uint8_t* loadBitmapFromPNG(const uint8_t *data, size_t dataSize, int* width, int* height) {
    int ncomp;
    uint8_t *pixels = stbi_load_from_memory(data, dataSize, width, height, &ncomp, 4);
    return pixels;
}
//  only call this with bitmapas loaded with loadBitmapFromPNG
void freeLoadedBitmap(uint8_t* data) {
    stbi_image_free(data);
}

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

uintptr_t loadImageFromPNG(const uint8_t *data, size_t dataSize, int* width, int* height) {
    auto image = loadImageFromPNG(data, dataSize);
    *width = image.width;
    *height = image.height;
    return image.image.id;
}

//  only call for images that will be thrown out (i.e. from loadImageFromPNG)
void freeLoadedImage(uintptr_t imageId) {
    sg_destroy_image(sg_image{(uint32_t)imageId});
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
    g_allImages[kFolder] = loadImageFromPNG(folder_solid_png, folder_solid_png_len);
    g_allImages[kDisk35] = loadImageFromPNG(disk_35_png, disk_35_png_len);
    g_allImages[kDisk525] = loadImageFromPNG(disk_525_png, disk_525_png_len);
    g_allImages[kDiskHDD] = loadImageFromPNG(hdd_png, hdd_png_len);
    g_allImages[kLockDisk] = loadImageFromPNG(lock_png, lock_png_len);
    g_allImages[kEjectDisk] = loadImageFromPNG(eject_png, eject_png_len);

    int lastNamedImageId = kFirstNamedImage;
    // g_allImages[lastNamedImageId] = loadImageFromPNG()
    for (; lastNamedImageId < kLastNamedImage; lastNamedImageId++) {
        g_allImages[lastNamedImageId].image.id = SG_INVALID_ID;
    }
}

uintptr_t getImage(ImageId imageId) { return g_allImages[imageId].image.id; }

float getImageAspect(ImageId imageId) {
    return float(g_allImages[imageId].width) / g_allImages[imageId].height;
}

int getImageWidth(ImageId imageId) { return float(g_allImages[imageId].width); }

int getImageHeight(ImageId imageId) { return float(g_allImages[imageId].height); }

ImageId getImageFromName(std::string_view name) {
    for (int i = kFirstNamedImage; i < kLastNamedImage; ++i) {
        if (g_namedImages[i - kFirstNamedImage] == NULL)
            break;
        if (!name.compare(g_namedImages[i - kFirstNamedImage])) {
            return ImageId(i);
        }
    }
    return kInvalidImageId;
}

void terminate() {
    for (int i = 0; i < kImageCount; ++i) {
        sg_destroy_image(g_allImages[i].image);
        g_allImages[i].image.id = SG_INVALID_ID;
    }
}

} // namespace ClemensHostAssets
