#ifndef CLEM_HOST_ASSETS_HPP
#define CLEM_HOST_ASSETS_HPP

#include <cstdint>

namespace ClemensHostAssets {

enum ImageId {
    kPowerButton,
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
    kImageCount
};

void initialize();
uintptr_t getImage(ImageId imageId);
int getImageWidth(ImageId imageId);
int getImageHeight(ImageId imageId);
float getImageAspect(ImageId imageId);
void terminate();

}; // namespace ClemensHostAssets

#endif
