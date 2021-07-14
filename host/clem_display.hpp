#ifndef CLEM_HOST_DISPLAY_H
#define CLEM_HOST_DISPLAY_H

#include "clem_types.h"
#include "sokol/sokol_gfx.h"

struct ClemensDisplayVertex
{
  float pos[2];
  float uvs[2];
  uint32_t color;
  uint32_t pad;
};

struct ClemensDisplayVertexParams
{
  float render_dims[2];
  float display_ratio[2];
  float virtual_dims[2];
  float offsets[2];
};

class ClemensDisplayProvider
{
public:
  ClemensDisplayProvider();
  ~ClemensDisplayProvider();

private:
  friend class ClemensDisplay;

  using DrawVertex = ClemensDisplayVertex;
  using DisplayVertexParams = ClemensDisplayVertexParams;

  sg_image systemFontImage_;
  sg_image systemFontImageHi_;
  sg_image loresFont_;
  sg_image blankImage_;
  sg_shader textShader_;
  sg_shader backShader_;
  sg_shader hiresShader_;
  sg_pipeline textPipeline_;
  sg_pipeline backPipeline_;
  sg_pipeline hiresPipeline_;
};

// all rendering occurs to an offscreen render target that will be rendered
// as a texture to the UI
class ClemensDisplay
{
public:
  ClemensDisplay(ClemensDisplayProvider& provider);
  ~ClemensDisplay();

  void start(const ClemensMonitor& monitor, int screen_w, int screen_h);
  void finish(float* uvs);

  //  all memory blocks passed to render functions are assumed to be 64K banks
  //  from the emulator.  The 'video' structures represent scanline data
  //  containing offsets into these banks.
  //
  void renderText40Col(const ClemensVideo& video,
    const uint8_t* mainMemory,
    bool useAlternateCharacterSet);
  void renderText80Col(const ClemensVideo& video,
    const uint8_t* mainMemory, const uint8_t* auxMemory,
    bool useAlternateCharacterSet);

  void renderLoresGraphics(const ClemensVideo& video, const uint8_t* memory);

  void renderHiresGraphics(const ClemensVideo& video, const uint8_t* memory);


  // Returns the color texture for the display for rendering
  sg_image getScreenTarget() const { return screenTarget_; }

private:
  void renderTextPlane(const ClemensVideo& video, int columns, const uint8_t* memory,
                       int phase, bool useAlternateCharacterSet);
  void renderLoresPlane(const ClemensVideo& video, int columns, const uint8_t* memory,
                       int phase);

  ClemensDisplayProvider& provider_;

  using DrawVertex = ClemensDisplayVertex;
  using DisplayVertexParams = ClemensDisplayVertexParams;

  sg_buffer textVertexBuffer_;
  sg_buffer vertexBuffer_;
  sg_image colorArray_;
  sg_image graphicsTarget_;
  sg_image screenTarget_;
  sg_pass screenPass_;

  uint8_t* emulatorVideoBuffer_;
  float emulatorVideoDimensions_[2];
  float emulatorMonitorDimensions_[2];
  unsigned emulatorTextColor_;
};

#endif