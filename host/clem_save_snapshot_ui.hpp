#ifndef CLEM_HOST_SAVE_SNAPSHOT_UI_HPP
#define CLEM_HOST_SAVE_SNAPSHOT_UI_HPP

class ClemensBackend;

class ClemensSaveSnapshotUI {
  public:
    bool isStarted() const;
    void start(ClemensBackend *backend, bool isEmulatorRunning);
    bool frame(float width, float height, ClemensBackend *backend);
    void stop(ClemensBackend *backend);
    void succeeded();
    void fail();

  private:
    enum class Mode { None, PromptForName, WaitForResponse, Succeeded, Failed, Cancelled };

    Mode mode_ = Mode::None;
    bool interruptedExecution_ = false;
    char snapshotName_[64];
};

#endif
