#ifndef CLEM_HOST_ASSETS_HPP
#define CLEM_HOST_ASSETS_HPP

#include "sokol/sokol_gfx.h"

namespace ClemensHostAssets {

enum ImageId { kPowerButton, kPowerCycle, kJoystick, kLoad, kSave, kCard, kImageCount };

void initialize();
sg_image getImage(ImageId imageId);
void terminate();

}; // namespace ClemensHostAssets

#endif
