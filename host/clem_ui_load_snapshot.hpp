#ifndef CLEM_HOST_LOAD_SNAPSHOT_UI_HPP
#define CLEM_HOST_LOAD_SNAPSHOT_UI_HPP

#include <string>

class ClemensCommandQueue;

class ClemensLoadSnapshotUI {
  public:
    bool isStarted() const;
    void start(ClemensCommandQueue &backend, const std::string &snapshotDir,
               bool isEmulatorRunning);
    bool frame(float width, float height, ClemensCommandQueue &backend);
    void stop(ClemensCommandQueue &backend);
    void succeeded();
    void fail();

  private:
    enum class Mode { None, Browser, WaitForResponse, Succeeded, Failed, Cancelled };

    Mode mode_ = Mode::None;
    std::string snapshotDir_;
    bool interruptedExecution_ = false;
    char snapshotName_[128];
};

#endif
