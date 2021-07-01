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
  sg_shader textShader_;
  sg_pipeline textPipeline_;
};

// all rendering occurs to an offscreen render target that will be rendered
// as a texture to the UI
class ClemensDisplay
{
public:
  ClemensDisplay(ClemensDisplayProvider& provider);
  ~ClemensDisplay();


  void renderText40Col(const ClemensVideo& video,
    const uint8_t* mainMemory,
    bool useAlternateCharacterSet);
  void renderText80Col(const ClemensVideo& video,
    const uint8_t* mainMemory, const uint8_t* auxMemory,
    bool useAlternateCharacterSet);


  // Returns the color texture for the display for rendering
  sg_image getScreenTarget() const { return screenTarget_; }

private:
  void renderTextPlane(const ClemensVideo& video, int columns, const uint8_t* memory,
                       int phase, bool useAlternateCharacterSet);

  ClemensDisplayProvider& provider_;

  using DrawVertex = ClemensDisplayVertex;
  using DisplayVertexParams = ClemensDisplayVertexParams;

  sg_buffer textVertexBuffer_;
  sg_image screenTarget_;
  sg_pass screenPass_;
};

#endif
