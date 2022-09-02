#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "clem_host_shared.hpp"
#include "clem_display.hpp"
#include "clem_audio.hpp"
#include "cinek/buffer.hpp"
#include "cinek/fixedstack.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

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
  //  the backend state delegate is run on a separate thread and notifies
  //  when a frame has been published
  void backendStateDelegate(const ClemensBackendState& state);

private:
  ClemensDisplayProvider displayProvider_;
  ClemensDisplay display_;
  ClemensAudioDevice audio_;
  std::unique_ptr<ClemensBackend> backend_;

  //  These buffers are populated when the backend publishes the current
  //  emulator state.   Their contents are refreshed for every publish
  std::condition_variable framePublished_;
  std::mutex frameMutex_;
  uint64_t frameSeqNo_, frameLastSeqNo_;
  //  Toggle between frame memory buffers so that backendStateDelegate and frame
  //  write to and read from separate buffers, to minimize the time we need to
  //  keep the frame mutex between the two threads.
  struct FrameState {
    uint8_t* bankE0;
    uint8_t* bankE1;
    uint8_t* memoryView;
    uint8_t* audioBuffer;

    Clemens65C816 cpu;
    ClemensMonitor monitorFrame;
    ClemensVideo textFrame;
    ClemensVideo graphicsFrame;
    ClemensAudio audioFrame;

    unsigned vgcModeFlags;

    float fps;
    bool mmioWasInitialized;
  };
  cinek::FixedStack frameWriteMemory_;
  cinek::FixedStack frameReadMemory_;
  FrameState frameWriteState_;
  FrameState frameReadState_;

};

#endif
