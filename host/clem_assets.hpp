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

}; // namespace ClemensHostAssets

#endif
