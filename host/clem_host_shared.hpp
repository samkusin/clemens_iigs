#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_types.h"
#include "cinek/buffer.hpp"

#include <string>

struct ClemensBackendOutputText {
  int level;
  std::string text;
};

struct ClemensBackendState {
  const ClemensMachine* machine;
  double fps;
  uint64_t seqno;
  bool mmio_was_initialized;

  ClemensMonitor monitor;
  ClemensVideo text;
  ClemensVideo graphics;
  ClemensAudio audio;

  unsigned hostCPUID;

  const ClemensBackendOutputText* logBufferStart;
  const ClemensBackendOutputText* logBufferEnd;
};

#endif
