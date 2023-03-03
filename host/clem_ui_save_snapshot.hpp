#ifndef CLEM_HOST_SAVE_SNAPSHOT_UI_HPP
#define CLEM_HOST_SAVE_SNAPSHOT_UI_HPP

class ClemensCommandQueue;

class ClemensSaveSnapshotUI {
  public:
    bool isStarted() const;
    void start(ClemensCommandQueue &backend, bool isEmulatorRunning);
    bool frame(float width, float height, ClemensCommandQueue &backend);
    void stop(ClemensCommandQueue &backend);
    void succeeded();
    void fail();

  private:
    enum class Mode { None, PromptForName, WaitForResponse, Succeeded, Failed, Cancelled };

    Mode mode_ = Mode::None;
    bool interruptedExecution_ = false;
    char snapshotName_[128];
};

#endif
