#ifndef CLEM_HOST_STARTUP_VIEW_HPP
#define CLEM_HOST_STARTUP_VIEW_HPP

#include "clem_configuration.hpp"
#include "clem_host_view.hpp"

#include <filesystem>

class ClemensPreamble;

class ClemensStartupView : public ClemensHostView {
  public:
    ClemensStartupView();

    const ClemensConfiguration &getConfiguration() const { return config_; }

  public:
    ViewType getViewType() const final { return ViewType::Startup; }
    ViewType frame(int width, int height, double deltaTime, FrameAppInterop &interop) final;
    void input(ClemensInputEvent) final;
    void lostFocus() final;

  private:
    enum class Mode {
        Initial,
        ChangeDataDirectory,
        Preamble,
        ROMCheck,
        ROMFileBrowser,
        SetupError,
        Aborted,
        Finished
    };
    bool validateDirectories();

    //  if ROM file exists, == 1, if no ROM file configured == 0,
    //  if filename points to a nonexistent ROM file, -1
    int checkROMFile();

    Mode mode_;
    ClemensConfiguration config_;

    std::string setupError_;

    std::unique_ptr<ClemensPreamble> preamble_;
};

#endif
