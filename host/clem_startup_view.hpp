#ifndef CLEM_HOST_STARTUP_VIEW_HPP
#define CLEM_HOST_STARTUP_VIEW_HPP

#include "clem_configuration.hpp"
#include "clem_host_view.hpp"

#include <filesystem>

class ClemensPreamble;

class ClemensStartupView : public ClemensHostView {
  public:
    ClemensStartupView(ClemensConfiguration& config);

  public:
    ViewType getViewType() const final { return ViewType::Startup; }
    ViewType frame(int width, int height, double deltaTime, ClemensHostInterop &interop) final;
    void input(ClemensInputEvent) final;
    bool emulatorHasFocus() const final { return false; }
    void pasteText(const char *text, unsigned textSizeLimit) final;
    void lostFocus() final;
    void gainFocus() final;

  private:
    enum class Mode { Initial, ChangeDataDirectory, Preamble, SetupError, Aborted, Finished };
    bool validateDirectories();

    Mode mode_;
    ClemensConfiguration& config_;

    std::string setupError_;

    std::unique_ptr<ClemensPreamble> preamble_;
};

#endif
