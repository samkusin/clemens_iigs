#ifndef CLEM_HOST_SETTINGS_UI_HPP
#define CLEM_HOST_SETTINGS_UI_HPP

class ClemensBackend;

class ClemensSettingsUI {
  public:
    bool isStarted() const;
    void start(ClemensBackend *backend);
    bool frame(float width, float height, ClemensBackend *backend);
    void stop(ClemensBackend *backend);

  private:
    enum class Mode { None, Active, Commit, Cancelled };
    Mode mode_ = Mode::None;
};

#endif
