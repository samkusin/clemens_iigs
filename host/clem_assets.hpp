#ifndef CLEM_HOST_ASSETS_HPP
#define CLEM_HOST_ASSETS_HPP

#include <cstdint>
#include <string_view>

namespace ClemensHostAssets {

enum ImageId {
    kInvalidImageId = -1,
    kPowerButton = 0,
    kPowerCycle,
    kSettings,
    kHelp,
    kDebugger,
    kLoad,
    kSave,
    kRunMachine,
    kStopMachine,
    kJoystick,
    kCard,
    kFastEmulate,
    kFolder,
    kDisk35,
    kDisk525,
    kDiskHDD,
    kLockDisk,
    kEjectDisk,
    kFirstNamedImage,
    kLastNamedImage = kFirstNamedImage + 64,
    kImageCount
};

void initialize();
uintptr_t getImage(ImageId imageId);
int getImageWidth(ImageId imageId);
int getImageHeight(ImageId imageId);
float getImageAspect(ImageId imageId);
ImageId getImageFromName(std::string_view name);
void terminate();


//  this can be freed with freeLoadedBitmap();
uint8_t* loadBitmapFromPNG(const uint8_t *data, size_t dataSize, int* width, int* height);
//  only call this with bitmapas loaded with loadBitmapFromPNG
void freeLoadedBitmap(uint8_t* data);
//  dynamically create texture images from PNG data
uintptr_t loadImageFromPNG(const uint8_t *data, size_t dataSize, int* width, int* height);
//  only call for images that will be thrown out (i.e. from loadImageFromPNG)
void freeLoadedImage(uintptr_t imageId);



}; // namespace ClemensHostAssets

#endif
