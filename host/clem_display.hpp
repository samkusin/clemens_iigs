#ifndef CLEM_HOST_DISPLAY_H
#define CLEM_HOST_DISPLAY_H

#include "cinek/buffer.hpp"
#include "clem_mmio_types.h"
#include "sokol/sokol_gfx.h"

struct ClemensDisplayVertex {
    float pos[2];
    float uvs[2];
    uint32_t color;
    uint32_t pad;
};

struct ClemensDisplayVertexParams {
    float render_dims[2];
    float display_ratio[2];
    float virtual_dims[2];
    float offsets[2];
};

class ClemensDisplayProvider {
  public:
    ClemensDisplayProvider(const cinek::ByteBuffer &systemFontLoBuffer,
                           const cinek::ByteBuffer &systemFontHiBuffer);
    ~ClemensDisplayProvider();

    void *allocate(size_t sz);
    void free(void *ptr);

  private:
    friend class ClemensDisplay;

    using DrawVertex = ClemensDisplayVertex;
    using DisplayVertexParams = ClemensDisplayVertexParams;

    sg_image systemFontImage_;
    sg_image systemFontImageHi_;
    sg_image blankImage_;
    sg_shader textShader_;
    sg_shader backShader_;
    sg_shader hiresShader_;
    sg_shader superHiresShader_; // TODO: consolidate with the hires one
    sg_pipeline textPipeline_;
    sg_pipeline backPipeline_;
    sg_pipeline hiresPipeline_;
    sg_pipeline superHiresPipeline_;
};

// all rendering occurs to an offscreen render target that will be rendered
// as a texture to the UI
class ClemensDisplay {
  public:
    ClemensDisplay(ClemensDisplayProvider &provider);
    ~ClemensDisplay();

    void start(const ClemensMonitor &monitor, int screen_w, int screen_h);
    void finish(float *uvs);

    //  all memory blocks passed to render functions are assumed to be 64K banks
    //  from the emulator.  The 'video' structures represent scanline data
    //  containing offsets into these banks.
    //
    void renderTextGraphics(const ClemensVideo &text, const ClemensVideo &graphics,
                            const uint8_t *mainMemory, const uint8_t *auxMemory, bool text80col,
                            bool altCharSet);
    void renderHiresGraphics(const ClemensVideo &video, const uint8_t *memory);
    void renderDoubleHiresGraphics(const ClemensVideo &video, const uint8_t *main,
                                   const uint8_t *aux);
    void renderSuperHiresGraphics(const ClemensVideo &video, const uint8_t *memory);

    // Returns the color texture for the display for rendering
    sg_image getScreenTarget() const { return screenTarget_; }

  private:
    using DrawVertex = ClemensDisplayVertex;
    using DisplayVertexParams = ClemensDisplayVertexParams;

    DrawVertex *renderTextPlane(DrawVertex *vertices, const ClemensVideo &video,
                                const DisplayVertexParams &vertexParams, int columns,
                                const uint8_t *memory, int phase, bool useAlternateCharacterSet);
    DrawVertex *renderLoresPlane(DrawVertex *vertices, const ClemensVideo &video,
                                 const DisplayVertexParams &vertexParams, int columns,
                                 const uint8_t *memory, int phase);
    void renderHiresGraphicsTexture(const ClemensVideo &video, const DisplayVertexParams &params,
                                    sg_image colorArray);
    DisplayVertexParams createVertexParams(float virtualDimX, float virtualDimY);

    ClemensDisplayProvider &provider_;

    sg_buffer textVertexBuffer_;
    sg_buffer vertexBuffer_;
    sg_image hgrColorArray_;
    sg_image dblhgrColorArray_;
    sg_image rgbaColorArray_;
    sg_image graphicsTarget_;
    sg_image screenTarget_;
    sg_pass screenPass_;

    sg_range textVertices_;

    uint8_t *emulatorVideoBuffer_;
    uint8_t *emulatorRGBABuffer_;
    float emulatorVideoDimensions_[2];
    float emulatorMonitorDimensions_[2];
    unsigned emulatorTextColor_;
    unsigned emulatorSignal_;
    unsigned emulatorColor_;
};

#endif
