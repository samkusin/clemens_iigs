#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_types.h"

struct ClemensBackendState {
  const ClemensMachine* machine;
  double fps;
  uint64_t seqno;
  bool mmio_was_initialized;

  Clemens65C816 cpu;
  ClemensMonitor monitor;
  ClemensVideo text;
  ClemensVideo graphics;
  ClemensAudio audio;

};

#endif
