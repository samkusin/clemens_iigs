#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "ckaudio/core.h"
#include "clem_types.h"

#include "clem_audio.hpp"

#include <memory>

class ClemensBackend;

class ClemensFrontend {
public:
  ClemensFrontend();
  ~ClemensFrontend();

  //  application rendering hook
  void frame(int width, int height, float deltaTime);
  //  application input from OS
  void input(const ClemensInputEvent &input);

private:
  ClemensAudioDevice audio_;
  std::unique_ptr<ClemensBackend> backend_;
};

#endif
