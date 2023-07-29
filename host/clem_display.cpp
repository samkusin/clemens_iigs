#include "clem_display.hpp"
#include "cinek/buffer.hpp"

#include "render.h"

#if defined(__GNUC__)
//  removes a lot of unused STB Truetype functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "stb_truetype.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(CK3D_BACKEND_D3D11)
#include "shaders/d3d11.inl"
#elif defined(CK3D_BACKEND_GL)
#include "shaders/glcore33.inl"
#endif

#include "sokol/sokol_gfx_ext.h"

//  Renders ClemensVideo data onto a render target/texture representing the
//  machine's screen
//
//  ClemensVideo comes in two packages: text and graphics.  This method allows
//  us to render the Apple IIgs mixed video modes.
//

static const int kDisplayTextColumnLimit = 80;
static const int kDisplayTextRowLimit = 24;

static stbtt_bakedchar kGlyphSet40col[512];
static stbtt_bakedchar kGlyphSet80col[512];

static unsigned kPrimarySetToGlyph[256];
static unsigned kAlternateSetToGlyph[256];

static int kFontTextureWidth = 512;
static int kFontTextureHeight = 256;

static int kGraphicsTextureWidth = 1024;
static int kGraphicsTextureHeight = 512;

static int kRenderTargetWidth = 1024;
static int kRenderTargetHeight = 512;

static int kColorTexelSize = 4;
static int kColorTextureWidth = 16 * kColorTexelSize;
static int kColorTextureHeight = 256 * kColorTexelSize;

namespace {

//  NTSC and IIgs versions
//  source from https://www.mrob.com/pub/xapple2/colors.html
//  RGBA (ABGR in little endian)
//
//  TODO: might be nice to configure "Apple II" hires colors as IIgs vs original
//

/*
//  Apple ][ colors
static const float kHiresColors[8][4] = {
  { 0x00, 0x00, 0x00, 0xFF },       // black group 1
  { 0x14, 0xF5, 0x3C, 0xFF },       // green
  { 0xFF, 0x44, 0xFC, 0xFF },       // purple/violet
  { 0xFF, 0xFF, 0xFF, 0xFF },       // white group 1
  { 0x00, 0x00, 0x00, 0xFF },       // black group 2
  { 0xFF, 0x6A, 0x3C, 0xFF },       // orange
  { 0x14, 0xCF, 0xFC, 0xFF },       // blue
  { 0xFF, 0xFF, 0xFF, 0xFF }        // white group 2
};
*/

//  Apple IIgs colors
static const uint8_t kHiresColors[8][4] = {
    {0x00, 0x00, 0x00, 0xFF}, // black group 1
    {0x11, 0xDD, 0x00, 0xFF}, // green (light green)
    {0xDD, 0x22, 0xDD, 0xFF}, // purple
    {0xFF, 0xFF, 0xFF, 0xFF}, // white group 1
    {0x00, 0x00, 0x00, 0xFF}, // black group 2
    {0xFF, 0x66, 0x00, 0xFF}, // orange
    {0x22, 0x22, 0xFF, 0xFF}, // medium blue
    {0xFF, 0xFF, 0xFF, 0xFF}  // white group 2
};

//  Double Hi-Res Graphics (better explanation than the reference books which
//  skirt the softswitches and video details)
//  http://www.1000bit.it/support/manuali/apple/technotes/aiie/tn.aiie.03.html

//  And a good description of the color pecularities between different monitors
//  and systems (//e vs IIgs) is here.
//  https://lukazi.blogspot.com/2017/03/double-high-resolution-graphics-dhgr.html
//
//  For this reason, the implementation can't promise to be IIgs accurate, or
//  even //e accurate
//
//  The hardware reference mentions a 'sliding window', which kind of follows
//  what has been done with our high-res graphics implementation.  See the
//  double hi-res plotter for details on the eventual implementation.
//

static const uint8_t kDblHiresColors[16][4] = {
    {0, 0, 0, 255},       // black
    {221, 0, 51, 255},    // deep red
    {136, 85, 0, 255},    // brown
    {255, 102, 0, 255},   // orange
    {0, 119, 34, 255},    // dark green
    {85, 85, 85, 255},    // dark gray
    {17, 221, 0, 255},    // lt. green
    {255, 255, 0, 255},   // yellow
    {0, 0, 153, 255},     // dark blue
    {221, 34, 221, 255},  // purple
    {170, 170, 170, 255}, // lt. gray
    {255, 153, 136, 255}, // pink
    {34, 34, 255, 255},   // med blue
    {102, 170, 255, 255}, // light blue
    {68, 255, 153, 255},  // aquamarine
    {255, 255, 255, 255}  // white
};

static const uint8_t kGr16Colors[16][4] = {
    {0, 0, 0, 255},       // black
    {221, 0, 51, 255},    // deep red
    {0, 0, 153, 255},     // dark blue
    {221, 34, 221, 255},  // purple
    {0, 119, 34, 255},    // dark green
    {85, 85, 85, 255},    // dark gray
    {34, 34, 255, 255},   // med blue
    {102, 170, 255, 255}, // light blue
    {136, 85, 0, 255},    // brown
    {255, 102, 0, 255},   // orange
    {170, 170, 170, 255}, // lt. gray
    {255, 153, 136, 255}, // pink
    {17, 221, 0, 255},    // lt. green
    {255, 255, 0, 255},   // yellow
    {68, 255, 153, 255},  // aquamarine
    {255, 255, 255, 255}  // white
};

uint32_t grColorToABGR(unsigned color) {
    const uint8_t *grColor = &kGr16Colors[color][0];
    unsigned abgr = ((unsigned)grColor[3] << 24) | ((unsigned)grColor[2] << 16) |
                    ((unsigned)grColor[1] << 8) | grColor[0];
    return abgr;
} // namespace

static sg_image loadFont(stbtt_bakedchar *glyphSet, const cinek::ByteBuffer &fileBuffer) {
    unsigned char *textureData = (unsigned char *)malloc(kFontTextureWidth * kFontTextureHeight);

    stbtt_BakeFontBitmap(fileBuffer.getHead(), 0, 16.0f, textureData, kFontTextureWidth,
                         kFontTextureHeight, 0xe000, 512, glyphSet);

    sg_image_desc imageDesc = {};
    imageDesc.width = kFontTextureWidth;
    imageDesc.height = kFontTextureHeight;
    imageDesc.pixel_format = SG_PIXELFORMAT_R8;
    imageDesc.min_filter = SG_FILTER_LINEAR;
    imageDesc.mag_filter = SG_FILTER_LINEAR;
    imageDesc.usage = SG_USAGE_IMMUTABLE;
    imageDesc.data.subimage[0][0].ptr = textureData;
    imageDesc.data.subimage[0][0].size = imageDesc.width * imageDesc.height;
    sg_image fontImage = sg_make_image(imageDesc);

    free(textureData);

    return fontImage;
}

void defineUniformBlocks(sg_shader_desc &shaderDesc) {
    shaderDesc.vs.uniform_blocks[0].size = sizeof(ClemensDisplayVertexParams);

#if defined(CK3D_BACKEND_GL)
    shaderDesc.vs.uniform_blocks[0].uniforms[0].name = "render_dims";
    shaderDesc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;
    shaderDesc.vs.uniform_blocks[0].uniforms[1].name = "display_ratio";
    shaderDesc.vs.uniform_blocks[0].uniforms[1].type = SG_UNIFORMTYPE_FLOAT2;
    shaderDesc.vs.uniform_blocks[0].uniforms[2].name = "virtual_dims";
    shaderDesc.vs.uniform_blocks[0].uniforms[2].type = SG_UNIFORMTYPE_FLOAT2;
    shaderDesc.vs.uniform_blocks[0].uniforms[3].name = "offsets";
    shaderDesc.vs.uniform_blocks[0].uniforms[3].type = SG_UNIFORMTYPE_FLOAT2;
#endif

#if defined(CK3D_BACKEND_D3D11)
    shaderDesc.attrs[0].sem_name = "POSITION";
    shaderDesc.attrs[1].sem_name = "TEXCOORD";
    shaderDesc.attrs[1].sem_index = 1;
    shaderDesc.attrs[2].sem_name = "COLOR";
    shaderDesc.attrs[2].sem_index = 1;
#endif
}

} // namespace

ClemensDisplayProvider::ClemensDisplayProvider(const cinek::ByteBuffer &systemFontLoBuffer,
                                               const cinek::ByteBuffer &systemFontHiBuffer) {
    systemFontImage_ = loadFont(kGlyphSet40col, systemFontLoBuffer);
    systemFontImageHi_ = loadFont(kGlyphSet80col, systemFontHiBuffer);

    const uint8_t blankImageData[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    sg_image_desc imageDesc = {};
    imageDesc.width = 4;
    imageDesc.height = 4;
    imageDesc.pixel_format = SG_PIXELFORMAT_R8;
    imageDesc.min_filter = SG_FILTER_LINEAR;
    imageDesc.mag_filter = SG_FILTER_LINEAR;
    imageDesc.usage = SG_USAGE_IMMUTABLE;
    imageDesc.data.subimage[0][0].ptr = blankImageData;
    imageDesc.data.subimage[0][0].size = imageDesc.width * imageDesc.height;
    blankImage_ = sg_make_image(imageDesc);

    //  map screen byte code to glyph index
    //    a 32-bit word split into two 16-bit half-words.  half-word values will
    //    differ if the character code is flashing
    for (int i = 0; i < 0x20; ++i) {
        kPrimarySetToGlyph[i] = (0x140 + i) | ((0x140 + i) << 16);
        kPrimarySetToGlyph[i + 0x20] = (0x120 + i) | ((0x120 + i) << 16);
        kPrimarySetToGlyph[i + 0x40] = (0x40 + i) | ((0x140 + i) << 16);
        kPrimarySetToGlyph[i + 0x60] = (0x20 + i) | ((0x120 + i) << 16);
        kPrimarySetToGlyph[i + 0x80] = (0x40 + i) | ((0x40 + i) << 16);
        kPrimarySetToGlyph[i + 0xA0] = (0x20 + i) | ((0x20 + i) << 16);
        kPrimarySetToGlyph[i + 0xC0] = (0x40 + i) | ((0x40 + i) << 16);
        kPrimarySetToGlyph[i + 0xE0] = (0x60 + i) | ((0x60 + i) << 16);

        kAlternateSetToGlyph[i] = (0x140 + i) | ((0x140 + i) << 16);
        kAlternateSetToGlyph[i + 0x20] = (0x120 + i) | ((0x120 + i) << 16);
        kAlternateSetToGlyph[i + 0x40] = (0x80 + i) | ((0x80 + i) << 16);
        kAlternateSetToGlyph[i + 0x60] = (0x160 + i) | ((0x160 + i) << 16);
        kAlternateSetToGlyph[i + 0x80] = (0x40 + i) | ((0x40 + i) << 16);
        kAlternateSetToGlyph[i + 0xA0] = (0x20 + i) | ((0x20 + i) << 16);
        kAlternateSetToGlyph[i + 0xC0] = (0x40 + i) | ((0x40 + i) << 16);
        kAlternateSetToGlyph[i + 0xE0] = (0x60 + i) | ((0x60 + i) << 16);
    }

    //  create shader
    sg_shader_desc shaderDesc = {};
    defineUniformBlocks(shaderDesc);
    shaderDesc.vs.source = VS_VERTEX_SOURCE;
    shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
#if defined(CK3D_BACKEND_GL)
    shaderDesc.fs.images[0].name = "tex";
#endif
    shaderDesc.fs.source = FS_TEXT_SOURCE;
    textShader_ = sg_make_shader(shaderDesc);

    //  create text pipeline and vertex buffer, no alpha blending, triangles
    sg_pipeline_desc renderPipelineDesc = {};
    renderPipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    renderPipelineDesc.layout.buffers[0].stride = sizeof(DrawVertex);
    renderPipelineDesc.shader = textShader_;
    renderPipelineDesc.cull_mode = SG_CULLMODE_BACK;
    renderPipelineDesc.face_winding = SG_FACEWINDING_CCW;
    renderPipelineDesc.colors[0].blend.enabled = true;
    renderPipelineDesc.colors[0].write_mask = SG_COLORMASK_RGB;
    renderPipelineDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    renderPipelineDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    renderPipelineDesc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    textPipeline_ = sg_make_pipeline(renderPipelineDesc);

    //  create hires pipeline and vertex buffer, no alpha blending, triangles
    shaderDesc = {};
    defineUniformBlocks(shaderDesc);
    shaderDesc.vs.source = VS_VERTEX_SOURCE;
    shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
    shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
#if defined(CK3D_BACKEND_GL)
    shaderDesc.fs.images[0].name = "hgr_tex";
    shaderDesc.fs.images[1].name = "hcolor_tex";
#endif
    shaderDesc.fs.source = FS_HIRES_SOURCE;
    hiresShader_ = sg_make_shader(shaderDesc);

    renderPipelineDesc = {};
    renderPipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    renderPipelineDesc.layout.buffers[0].stride = sizeof(DrawVertex);
    renderPipelineDesc.shader = hiresShader_;
    renderPipelineDesc.cull_mode = SG_CULLMODE_BACK;
    renderPipelineDesc.face_winding = SG_FACEWINDING_CCW;
    renderPipelineDesc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    hiresPipeline_ = sg_make_pipeline(renderPipelineDesc);

    //  create super hires pipeline and vertex buffer, no alpha blending, triangles
    shaderDesc = {};
    defineUniformBlocks(shaderDesc);
    shaderDesc.vs.source = VS_VERTEX_SOURCE;
    shaderDesc.fs.uniform_blocks[0].size = sizeof(ClemensDisplayFragmentParams);
    shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
    shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
#if defined(CK3D_BACKEND_GL)
    shaderDesc.fs.uniform_blocks[0].uniforms[0].name = "screen_params";
    shaderDesc.fs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    shaderDesc.fs.uniform_blocks[0].uniforms[1].name = "color_params";
    shaderDesc.fs.uniform_blocks[0].uniforms[1].type = SG_UNIFORMTYPE_FLOAT4;
    shaderDesc.fs.images[0].name = "screen_tex";
    shaderDesc.fs.images[1].name = "color_tex";
#endif
    shaderDesc.fs.source = FS_SUPER_SOURCE;
    superHiresShader_ = sg_make_shader(shaderDesc);

    renderPipelineDesc = {};
    renderPipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    renderPipelineDesc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    renderPipelineDesc.layout.buffers[0].stride = sizeof(DrawVertex);
    renderPipelineDesc.shader = superHiresShader_;
    renderPipelineDesc.cull_mode = SG_CULLMODE_BACK;
    renderPipelineDesc.face_winding = SG_FACEWINDING_CCW;
    renderPipelineDesc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    superHiresPipeline_ = sg_make_pipeline(renderPipelineDesc);
}

ClemensDisplayProvider::~ClemensDisplayProvider() {
    sg_destroy_pipeline(superHiresPipeline_);
    sg_destroy_shader(superHiresShader_);
    sg_destroy_pipeline(hiresPipeline_);
    sg_destroy_shader(hiresShader_);
    sg_destroy_pipeline(textPipeline_);
    sg_destroy_shader(textShader_);
    sg_destroy_image(systemFontImageHi_);
    sg_destroy_image(systemFontImage_);
    sg_destroy_image(blankImage_);
}

void *ClemensDisplayProvider::allocate(size_t sz) { return ::malloc(sz); }

void ClemensDisplayProvider::free(void *ptr) { ::free(ptr); }

ClemensDisplay::ClemensDisplay(ClemensDisplayProvider &provider) : provider_(provider) {
    //  This data is specific to display and should be instanced per display
    //  require double the vertices to display the background under the
    //  foreground
    textVertices_.size =
        4 * (kDisplayTextRowLimit * kDisplayTextColumnLimit * 6) * (sizeof(DrawVertex));
    textVertices_.ptr = provider_.allocate(textVertices_.size);

    sg_buffer_desc vertexBufDesc = {};
    //  using 2 x 2 x 40x24 quads (lores has two pixels per box)
    vertexBufDesc.usage = SG_USAGE_STREAM;
    vertexBufDesc.size = textVertices_.size;
    textVertexBuffer_ = sg_make_buffer(&vertexBufDesc);

    vertexBufDesc = {};
    //  simple quad -TODO:  do we need this?
    vertexBufDesc.usage = SG_USAGE_STREAM;
    vertexBufDesc.size = 6 * sizeof(DrawVertex);
    vertexBuffer_ = sg_make_buffer(&vertexBufDesc);

    //  sokol doesn't support Texture1D out of the box, so fake it with a 2D
    //  abgr color texture of 8 vertical lines, 8 pixels high.
    uint8_t hiresColorData[32 * 8];
    for (int y = 0; y < 8; ++y) {
        uint8_t *texdata = &hiresColorData[32 * y];
        for (int x = 0; x < 8; ++x) {
            texdata[x * 4] = kHiresColors[x][0];
            texdata[x * 4 + 1] = kHiresColors[x][1];
            texdata[x * 4 + 2] = kHiresColors[x][2];
            texdata[x * 4 + 3] = kHiresColors[x][3];
        }
    }

    sg_image_desc imageDesc = {};
    imageDesc.width = 8;
    imageDesc.height = 8;
    imageDesc.type = SG_IMAGETYPE_2D;
    imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    imageDesc.min_filter = SG_FILTER_NEAREST;
    imageDesc.mag_filter = SG_FILTER_NEAREST;
    imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.usage = SG_USAGE_IMMUTABLE;
    imageDesc.data.subimage[0][0].ptr = hiresColorData;
    imageDesc.data.subimage[0][0].size = sizeof(hiresColorData);
    hgrColorArray_ = sg_make_image(imageDesc);

    uint8_t dblHiresColorData[64 * 8];
    for (int y = 0; y < 8; ++y) {
        uint8_t *texdata = &dblHiresColorData[64 * y];
        for (int x = 0; x < 16; ++x) {
            texdata[x * 4] = kDblHiresColors[x][0];
            texdata[x * 4 + 1] = kDblHiresColors[x][1];
            texdata[x * 4 + 2] = kDblHiresColors[x][2];
            texdata[x * 4 + 3] = kDblHiresColors[x][3];
        }
    }

    imageDesc = {};
    imageDesc.width = 16;
    imageDesc.height = 8;
    imageDesc.type = SG_IMAGETYPE_2D;
    imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    imageDesc.min_filter = SG_FILTER_NEAREST;
    imageDesc.mag_filter = SG_FILTER_NEAREST;
    imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.usage = SG_USAGE_IMMUTABLE;
    imageDesc.data.subimage[0][0].ptr = dblHiresColorData;
    imageDesc.data.subimage[0][0].size = sizeof(dblHiresColorData);
    dblhgrColorArray_ = sg_make_image(imageDesc);

    emulatorRGBABuffer_ = new uint8_t[kColorTextureWidth * kColorTextureHeight * 4];
    memset(emulatorRGBABuffer_, 0, kColorTextureWidth * kColorTextureHeight * 4);

    imageDesc = {};
    imageDesc.width = kColorTextureWidth;
    imageDesc.height = kColorTextureHeight;
    imageDesc.type = SG_IMAGETYPE_2D;
    imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    imageDesc.min_filter = SG_FILTER_NEAREST;
    imageDesc.mag_filter = SG_FILTER_NEAREST;
    imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.usage = SG_USAGE_STREAM;
    rgbaColorArray_ = sg_make_image(imageDesc);

    imageDesc = {};
    imageDesc.width = kGraphicsTextureWidth;
    imageDesc.height = kGraphicsTextureHeight;
    imageDesc.pixel_format = SG_PIXELFORMAT_R8;
    imageDesc.min_filter = SG_FILTER_NEAREST;
    imageDesc.mag_filter = SG_FILTER_NEAREST;
    imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.usage = SG_USAGE_STREAM;
    graphicsTarget_ = sg_make_image(imageDesc);
    emulatorVideoBuffer_ = new uint8_t[kGraphicsTextureWidth * kGraphicsTextureHeight];

    //  create offscreen pass and image targets
    // const int rtSampleCount = sg_query_features().msaa_render_targets ?
    imageDesc = {};
    imageDesc.render_target = true;
    imageDesc.width = kRenderTargetWidth;
    imageDesc.height = kRenderTargetHeight;
    imageDesc.min_filter = SG_FILTER_LINEAR;
    imageDesc.mag_filter = SG_FILTER_LINEAR;
    imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    imageDesc.sample_count = 1;
    screenTarget_ = sg_make_image(imageDesc);

    sg_pass_desc passDesc = {};
    passDesc.color_attachments[0].image = screenTarget_;
    screenPass_ = sg_make_pass(passDesc);
}

ClemensDisplay::~ClemensDisplay() {
    sg_destroy_pass(screenPass_);
    sg_destroy_image(screenTarget_);
    sg_destroy_image(graphicsTarget_);
    sg_destroy_image(hgrColorArray_);
    sg_destroy_image(dblhgrColorArray_);
    sg_destroy_image(rgbaColorArray_);
    sg_destroy_buffer(vertexBuffer_);
    sg_destroy_buffer(textVertexBuffer_);
    provider_.free((void *)textVertices_.ptr);
    delete[] emulatorVideoBuffer_;
    delete[] emulatorRGBABuffer_;
}

void ClemensDisplay::start(const ClemensMonitor &monitor, int screen_w, int screen_h) {
    sg_pass_action passAction = {};
    passAction.colors[0].action = SG_ACTION_CLEAR;
    const uint8_t *borderColor = &kGr16Colors[monitor.border_color & 0xf][0];
    passAction.colors[0].value = {borderColor[0] / 255.0f, borderColor[1] / 255.0f,
                                  borderColor[2] / 255.0f, 1.0f};

    sg_begin_pass(screenPass_, &passAction);

    emulatorMonitorDimensions_[0] = screen_w;
    emulatorMonitorDimensions_[1] = screen_h;

    emulatorVideoDimensions_[0] = monitor.width;
    emulatorVideoDimensions_[1] = monitor.height;

    emulatorTextColor_ = monitor.text_color;
    emulatorSignal_ = monitor.signal;
    emulatorColor_ = monitor.color;
}

void ClemensDisplay::finish(float *uvs) {
    sg_end_pass();
    uvs[0] = emulatorMonitorDimensions_[0] / kRenderTargetWidth;
    uvs[1] = emulatorMonitorDimensions_[1] / kRenderTargetHeight;
}

std::vector<unsigned char> ClemensDisplay::capture(int *w, int *h) {
    std::vector<unsigned char> buffer(kRenderTargetWidth * kRenderTargetHeight * 4);
    //  copy whole texture!
    sg_query_image_pixels(screenTarget_, buffer.data(), buffer.size());
    //  compress to only needed pixels based on width and height of monitor
    int width = int(std::round(emulatorMonitorDimensions_[0]));
    int height = int(std::round(emulatorMonitorDimensions_[1]));
    const size_t srcPitch = kRenderTargetWidth * 4;
    size_t toOffset = width * 4;
    for (int row = 1, offset = srcPitch; row < height; row++) {
        std::copy(buffer.data() + offset, buffer.data() + offset + srcPitch,
                  buffer.data() + toOffset);
        offset += srcPitch;
        toOffset += width * 4;
    }
    buffer.resize(toOffset);
    buffer.shrink_to_fit();
    *w = width;
    *h = height;
    return buffer;
}

bool ClemensDisplay::shouldFlipTarget() const {
#if defined(CK3D_BACKEND_GL)
    return true;
#else
    return false;
#endif
}

void ClemensDisplay::renderTextGraphics(const ClemensVideo &text, const ClemensVideo &graphics,
                                        const uint8_t *mainMemory, const uint8_t *auxMemory,
                                        bool text80col, bool useAltCharSet) {

    DrawVertex *verticesBegin =
        reinterpret_cast<DrawVertex *>(const_cast<void *>(textVertices_.ptr));
    DrawVertex *vertices = verticesBegin;
    DisplayVertexParams textVertexParams;
    DisplayVertexParams loresVertexParams;

    DrawVertex *loresVertices = vertices;
    if (graphics.format == kClemensVideoFormat_Double_Lores) {
        loresVertexParams = createVertexParams(80, 48);
        vertices = renderLoresPlane(vertices, graphics, loresVertexParams, 80, auxMemory, 0);
        if (!vertices)
            return;
        vertices = renderLoresPlane(vertices, graphics, loresVertexParams, 80, mainMemory, 1);
        if (!vertices)
            return;
    } else if (graphics.format == kClemensVideoFormat_Lores) {
        loresVertexParams = createVertexParams(40, 48);
        vertices = renderLoresPlane(vertices, graphics, loresVertexParams, 40, mainMemory, 0);
    }

    DrawVertex *textVertices = vertices;
    if (text.format == kClemensVideoFormat_Text) {
        if (text80col) {
            textVertexParams = createVertexParams(80, 24);
            vertices =
                renderTextPlane(vertices, text, textVertexParams, 80, auxMemory, 0, useAltCharSet);
            if (!vertices)
                return;
            vertices =
                renderTextPlane(vertices, text, textVertexParams, 80, mainMemory, 1, useAltCharSet);
            if (!vertices)
                return;
        } else {
            textVertexParams = createVertexParams(40, 24);
            vertices =
                renderTextPlane(vertices, text, textVertexParams, 40, mainMemory, 0, useAltCharSet);
            if (!vertices)
                return;
        }
    }

    if ((vertices - verticesBegin) == 0)
        return;

    sg_apply_pipeline(provider_.textPipeline_);
    sg_update_buffer(textVertexBuffer_, textVertices_);

    //  render lores first
    sg_bindings backBindings = {};
    backBindings.fs_images[0] = provider_.blankImage_;
    backBindings.vertex_buffers[0] = textVertexBuffer_;
    int loresVertexCount = int(textVertices - loresVertices);
    if (loresVertexCount > 0) {
        int drawCount = graphics.format == kClemensVideoFormat_Double_Lores ? 2 : 1;
        assert((loresVertexCount % drawCount) == 0); // sanity check
        int vertexPerDrawCall = loresVertexCount / drawCount;

        sg_range uniformsBuffer = {};
        uniformsBuffer.ptr = &loresVertexParams;
        uniformsBuffer.size = sizeof(loresVertexParams);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, uniformsBuffer);
        sg_apply_bindings(backBindings);
        sg_draw(0, vertexPerDrawCall, 1);
        if (graphics.format == kClemensVideoFormat_Double_Lores) {
            sg_draw(vertexPerDrawCall, vertexPerDrawCall, 1);
        }
    }

    //  render the text background and character requiring two draw calls.  the
    //  background and text characters are not interleaved
    sg_bindings textBindings = {};
    textBindings.fs_images[0] =
        text80col ? provider_.systemFontImageHi_ : provider_.systemFontImage_;
    textBindings.vertex_buffers[0] = textVertexBuffer_;
    int textVertexOffset = loresVertexCount;
    int textVertexCount = int(vertices - textVertices);
    if (textVertexCount > 0) {
        int drawCount = text80col ? 4 : 2;
        assert((textVertexCount % drawCount) == 0); // sanity check
        int textVertexPerDrawCall = textVertexCount / drawCount;

        sg_range uniformsBuffer = {};
        uniformsBuffer.ptr = &textVertexParams;
        uniformsBuffer.size = sizeof(textVertexParams);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, uniformsBuffer);
        sg_apply_bindings(backBindings);
        sg_draw(textVertexOffset, textVertexPerDrawCall, 1);
        sg_apply_bindings(textBindings);
        sg_draw(textVertexOffset + textVertexPerDrawCall, textVertexPerDrawCall, 1);
        if (text80col) {
            sg_apply_bindings(backBindings);
            sg_draw(textVertexOffset + textVertexPerDrawCall * 2, textVertexPerDrawCall, 1);
            sg_apply_bindings(textBindings);
            sg_draw(textVertexOffset + textVertexPerDrawCall * 3, textVertexPerDrawCall, 1);
        }
    }
}

auto ClemensDisplay::renderTextPlane(DrawVertex *vertices, const ClemensVideo &video,
                                     const DisplayVertexParams &vertexParams, int columns,
                                     const uint8_t *memory, int phase,
                                     bool useAlternateCharacterSet) -> DrawVertex * {
    if (video.format != kClemensVideoFormat_Text) {
        return nullptr;
    }

    const int kPhaseCount = columns / 40;

    stbtt_bakedchar *glyphSet;
    if (columns == 80) {
        glyphSet = kGlyphSet80col;
    } else {
        glyphSet = kGlyphSet40col;
    }

    // pass 1 - background
    unsigned textABGR = grColorToABGR((emulatorTextColor_ >> 4) & 0xf);

    DrawVertex *vertex = vertices;
    for (int i = 0; i < video.scanline_count; ++i) {
        for (int j = 0; j < video.scanline_byte_cnt; ++j) {
            float x0 = ((j * kPhaseCount) + phase);
            float y0 = i + video.scanline_start;
            float x1 = x0 + 1.0f;
            float y1 = y0 + 1.0f;
            vertex[0] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[1] = {{x0, y1}, {0.0f, 1.0f}, textABGR};
            vertex[2] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[3] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[4] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[5] = {{x1, y0}, {1.0f, 0.0f}, textABGR};
            vertex += 6;
        }
    }

    //  poss 2 - foreground
    //  determine cycle for flashing characters - we use a vertical blank counter
    //  which in combination with the screen mode (NTSC vs PAL) can be used to
    //  calculate a real-time value
    unsigned monitorRefreshRate = emulatorSignal_ == CLEM_MONITOR_SIGNAL_PAL ? 50 : 60;
    unsigned videoTimePhase = video.vbl_counter % monitorRefreshRate;
    textABGR = grColorToABGR(emulatorTextColor_ & 0xf);
    for (int i = 0; i < video.scanline_count; ++i) {
        int row = i + video.scanline_start;
        const uint8_t *scanline = memory + video.scanlines[row].offset;
        for (int j = 0; j < video.scanline_byte_cnt; ++j) {
            unsigned glyphIndex;
            const uint8_t charIndex = scanline[j];
            if (useAlternateCharacterSet) {
                glyphIndex = kAlternateSetToGlyph[charIndex];
            } else {
                glyphIndex = kPrimarySetToGlyph[charIndex];
            }
            //  TODO: is the cycle really 1 second?
            if (videoTimePhase >= monitorRefreshRate / 2) {
                glyphIndex = (glyphIndex << 16) | (glyphIndex >> 16);
            }
            glyphIndex &= 0xffff;
            // auto* glyph = &glyphSet[glyphIndex];
            stbtt_aligned_quad quad;
            float xpos = ((j * kPhaseCount) + phase) * vertexParams.display_ratio[0];
            float ypos = row * vertexParams.display_ratio[1] + (vertexParams.display_ratio[1] - 1);
            stbtt_GetBakedQuad(glyphSet, kFontTextureWidth, kFontTextureHeight, glyphIndex, &xpos,
                               &ypos, &quad, 1);
            float l = quad.x0 / vertexParams.display_ratio[0];
            float r = quad.x1 / vertexParams.display_ratio[0];
            float t = quad.y0 / vertexParams.display_ratio[1];
            float b = quad.y1 / vertexParams.display_ratio[1];
            vertex[0] = {{l, t}, {quad.s0, quad.t0}, textABGR};
            vertex[1] = {{l, b}, {quad.s0, quad.t1}, textABGR};
            vertex[2] = {{r, b}, {quad.s1, quad.t1}, textABGR};
            vertex[3] = {{l, t}, {quad.s0, quad.t0}, textABGR};
            vertex[4] = {{r, b}, {quad.s1, quad.t1}, textABGR};
            vertex[5] = {{r, t}, {quad.s1, quad.t0}, textABGR};
            vertex += 6;
        }
    }

    return vertex;
}

auto ClemensDisplay::renderLoresPlane(DrawVertex *vertices, const ClemensVideo &video,
                                      const DisplayVertexParams &, int columns,
                                      const uint8_t *memory, int phase) -> DrawVertex * {
    if (video.format != kClemensVideoFormat_Lores) {
        return nullptr;
    }

    const int kPhaseCount = columns / 40;

    // background pass
    DrawVertex *vertex = vertices;
    for (int i = 0; i < video.scanline_count; ++i) {
        int row = i + video.scanline_start;
        const uint8_t *scanline = memory + video.scanlines[row].offset;
        for (int j = 0; j < video.scanline_byte_cnt; ++j) {
            float x0 = ((j * kPhaseCount) + phase);
            float y0 = i * 2;
            float x1 = x0 + 1.0f;
            float y1 = y0 + 1.0f;

            uint8_t block = scanline[j];
            unsigned textABGR = grColorToABGR(block & 0xf);
            vertex[0] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[1] = {{x0, y1}, {0.0f, 1.0f}, textABGR};
            vertex[2] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[3] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[4] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[5] = {{x1, y0}, {1.0f, 0.0f}, textABGR};
            vertex += 6;
            textABGR = grColorToABGR(block >> 4);
            y0 = y1;
            y1 += 1.0f;
            vertex[0] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[1] = {{x0, y1}, {0.0f, 1.0f}, textABGR};
            vertex[2] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[3] = {{x0, y0}, {0.0f, 0.0f}, textABGR};
            vertex[4] = {{x1, y1}, {1.0f, 1.0f}, textABGR};
            vertex[5] = {{x1, y0}, {1.0f, 0.0f}, textABGR};
            vertex += 6;
        }
    }

    return vertex;
}

void ClemensDisplay::renderHiresGraphics(const ClemensVideo &video, const uint8_t *memory) {
    if (video.format != kClemensVideoFormat_Hires) {
        return;
    }
    clemens_render_graphics(&video, memory, nullptr, emulatorVideoBuffer_, kGraphicsTextureWidth,
                            kGraphicsTextureHeight, kGraphicsTextureWidth);

    //  TODO: simplify vertex shader for graphics screens
    //        a lot of these uniforms don't seem  necessary, but we have to set
    //        them up so the shader works.
    auto vertexParams =
        createVertexParams(emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);
    renderHiresGraphicsTexture(video, vertexParams, hgrColorArray_);
}

void ClemensDisplay::renderDoubleHiresGraphics(const ClemensVideo &video, const uint8_t *main,
                                               const uint8_t *aux) {
    if (video.format != kClemensVideoFormat_Double_Hires) {
        return;
    }

    clemens_render_graphics(&video, main, aux, emulatorVideoBuffer_, kGraphicsTextureWidth,
                            kGraphicsTextureHeight, kGraphicsTextureWidth);

    auto vertexParams =
        createVertexParams(emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);
    renderHiresGraphicsTexture(video, vertexParams, dblhgrColorArray_);
}

void ClemensDisplay::renderHiresGraphicsTexture(const ClemensVideo &video,
                                                const DisplayVertexParams &vertexParams,
                                                sg_image colorArray) {
    sg_image_data graphicsImageData = {};
    graphicsImageData.subimage[0][0].ptr = emulatorVideoBuffer_;
    graphicsImageData.subimage[0][0].size = kGraphicsTextureWidth * kGraphicsTextureHeight;

    sg_update_image(graphicsTarget_, graphicsImageData);

    sg_range rangeParam;
    rangeParam.ptr = &vertexParams;
    rangeParam.size = sizeof(vertexParams);

    sg_apply_pipeline(provider_.hiresPipeline_);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

    //  texture contains a scaled version of the original 280 x 160/192 screen
    //  to avoid UV rounding issues
    DrawVertex vertices[6];
    float y_scalar = emulatorVideoDimensions_[1] / 192.0f;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = x0 + emulatorVideoDimensions_[0];
    float y1 = y0 + (video.scanline_count * y_scalar);
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = emulatorVideoDimensions_[0] / kGraphicsTextureWidth;
    float v1 = (video.scanline_count * y_scalar) / kGraphicsTextureHeight;

    sg_range verticesRange;
    verticesRange.ptr = &vertices[0];
    verticesRange.size = 6 * sizeof(DrawVertex);
    vertices[0] = {{x0, y0}, {u0, v0}, 0xffffffff};
    vertices[1] = {{x0, y1}, {u0, v1}, 0xffffffff};
    vertices[2] = {{x1, y1}, {u1, v1}, 0xffffffff};
    vertices[3] = {{x0, y0}, {u0, v0}, 0xffffffff};
    vertices[4] = {{x1, y1}, {u1, v1}, 0xffffffff};
    vertices[5] = {{x1, y0}, {u1, v0}, 0xffffffff};

    sg_bindings renderBindings = {};
    renderBindings.vertex_buffers[0] = vertexBuffer_;
    renderBindings.fs_images[0] = graphicsTarget_;
    renderBindings.fs_images[1] = colorArray;
    renderBindings.vertex_buffer_offsets[0] =
        (sg_append_buffer(renderBindings.vertex_buffers[0], verticesRange));
    sg_apply_bindings(renderBindings);
    sg_draw(0, 6, 1);
}

void ClemensDisplay::renderSuperHiresGraphics(const ClemensVideo &video, const uint8_t *memory) {
    // 1x2 pixels
    uint8_t *video_out = emulatorVideoBuffer_;
    clemens_render_graphics(&video, memory, nullptr, video_out, kGraphicsTextureWidth,
                            kGraphicsTextureHeight, kGraphicsTextureWidth);

    uint8_t *buffer0 = video_out;
    for (int y = 0; y < video.scanline_count; ++y) {
        uint8_t *buffer1 = buffer0 + kGraphicsTextureWidth;
        memcpy(buffer1, buffer0, kGraphicsTextureWidth);
        buffer0 += kGraphicsTextureWidth * 2;
    }

    //  generate palette from up to 3200 colors (16 colors per scanline)
    uint8_t *rgbaout = emulatorRGBABuffer_;
    const uint16_t *rgbpalette = video.rgb;
    for (int y = 0; y < video.scanline_count; ++y) {
        for (int yt = 0; yt < kColorTexelSize; ++yt) {
            uint8_t *rgboutRow = rgbaout;
            for (int x = 0; x < 16; ++x) {
                uint8_t red = uint8_t((rgbpalette[x] & 0x0f00) >> 8);
                uint8_t green = uint8_t(rgbpalette[x] & 0x00f0) >> 4;
                uint8_t blue = uint8_t(rgbpalette[x] & 0x000f);
                red |= (red << 4);
                green |= (green << 4);
                blue |= (blue << 4);
                *(rgboutRow++) = red;
                *(rgboutRow++) = green;
                *(rgboutRow++) = blue;
                *(rgboutRow++) = 0xff;
                *(rgboutRow++) = red;
                *(rgboutRow++) = green;
                *(rgboutRow++) = blue;
                *(rgboutRow++) = 0xff;
                *(rgboutRow++) = red;
                *(rgboutRow++) = green;
                *(rgboutRow++) = blue;
                *(rgboutRow++) = 0xff;
                *(rgboutRow++) = red;
                *(rgboutRow++) = green;
                *(rgboutRow++) = blue;
                *(rgboutRow++) = 0xff;
            }
            rgbaout += kColorTextureWidth * 4;
        }
        rgbpalette += 16;
    }
    // finally set up the color texture and shader/pipeline for rendering
    sg_image_data graphicsImageData = {};
    graphicsImageData.subimage[0][0].ptr = emulatorVideoBuffer_;
    graphicsImageData.subimage[0][0].size = kGraphicsTextureWidth * kGraphicsTextureHeight;
    sg_update_image(graphicsTarget_, graphicsImageData);

    graphicsImageData.subimage[0][0].ptr = emulatorRGBABuffer_;
    graphicsImageData.subimage[0][0].size = kColorTextureWidth * kColorTextureHeight * 4;
    sg_update_image(rgbaColorArray_, graphicsImageData);

    sg_apply_pipeline(provider_.superHiresPipeline_);
    sg_range rangeParam;
    auto vertexParams =
        createVertexParams(emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);
    rangeParam.ptr = &vertexParams;
    rangeParam.size = sizeof(vertexParams);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

    ClemensDisplayFragmentParams fragmentParams;
    //  all pixels are scaled 1x2 from the emulated display on the render target
    fragmentParams.screen_params[0] = kGraphicsTextureWidth;
    fragmentParams.screen_params[1] = kGraphicsTextureHeight;
    fragmentParams.screen_params[2] = 1;
    fragmentParams.screen_params[3] = 2;
    fragmentParams.color_params[0] = kColorTextureWidth;
    fragmentParams.color_params[1] = kColorTextureHeight;
    fragmentParams.color_params[2] = kColorTexelSize;
    fragmentParams.color_params[3] = kColorTexelSize;
    rangeParam.ptr = &fragmentParams;
    rangeParam.size = sizeof(fragmentParams);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, rangeParam);

    //  texture contains a scaled version of the original 640 x 200 screen
    //  to avoid UV rounding issues
    DrawVertex vertices[6];
    float y_scalar = emulatorVideoDimensions_[1] / 200.0f;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = x0 + emulatorVideoDimensions_[0];
    float y1 = y0 + (video.scanline_count * y_scalar);
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = emulatorVideoDimensions_[0] / kGraphicsTextureWidth;
    float v1 = (video.scanline_count * y_scalar) / kGraphicsTextureHeight;

    sg_range verticesRange;
    verticesRange.ptr = &vertices[0];
    verticesRange.size = 6 * sizeof(DrawVertex);
    vertices[0] = {{x0, y0}, {u0, v0}, 0xffffffff};
    vertices[1] = {{x0, y1}, {u0, v1}, 0xffffffff};
    vertices[2] = {{x1, y1}, {u1, v1}, 0xffffffff};
    vertices[3] = {{x0, y0}, {u0, v0}, 0xffffffff};
    vertices[4] = {{x1, y1}, {u1, v1}, 0xffffffff};
    vertices[5] = {{x1, y0}, {u1, v0}, 0xffffffff};

    sg_bindings renderBindings = {};
    renderBindings.vertex_buffers[0] = vertexBuffer_;
    renderBindings.fs_images[0] = graphicsTarget_;
    renderBindings.fs_images[1] = rgbaColorArray_;
    renderBindings.vertex_buffer_offsets[0] =
        (sg_append_buffer(renderBindings.vertex_buffers[0], verticesRange));
    sg_apply_bindings(renderBindings);
    sg_draw(0, 6, 1);
}

auto ClemensDisplay::createVertexParams(float virtualDimX, float virtualDimY)
    -> DisplayVertexParams {
    DisplayVertexParams vertexParams;
    vertexParams.virtual_dims[0] = virtualDimX;
    vertexParams.virtual_dims[1] = virtualDimY;
    vertexParams.display_ratio[0] = emulatorVideoDimensions_[0] / virtualDimX;
    vertexParams.display_ratio[1] = emulatorVideoDimensions_[1] / virtualDimY;
    vertexParams.render_dims[0] = kRenderTargetWidth;
    vertexParams.render_dims[1] = kRenderTargetHeight;
    vertexParams.offsets[0] = (emulatorMonitorDimensions_[0] - emulatorVideoDimensions_[0]) * 0.5f;
    vertexParams.offsets[1] = (emulatorMonitorDimensions_[1] - emulatorVideoDimensions_[1]) * 0.5f;
    return vertexParams;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
