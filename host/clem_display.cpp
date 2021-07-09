#include "clem_display.hpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "misc/stb_truetype.h"

#include <cstdio>
#include <cstdlib>

#include "shaders/d3d11.inl"


//  Renders ClemensVideo data onto a render target/texture representing the
//  machine's screen
//
//  ClemensVideo comes in two packages: text and graphics.  This method allows
//  us to render the Apple IIgs mixed video modes.
//
//  def Setup():
//    Create a texture for the Screen: (1024x256)
//
//  def DrawTextMode(ClemensVideo):
//    Iterate through the window described in ClemensVideo for each text
//    character.  Obtain character set information from MMIO VGC settings
//    passed by the emulator applciation
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
static const float kHiresColors[8][4] = {
  { 0x00, 0x00, 0x00, 0xFF },       // black group 1
  { 0x11, 0xDD, 0x00, 0xFF },       // green (light green)
  { 0xDD, 0x22, 0xDD, 0xFF },       // purple
  { 0xFF, 0xFF, 0xFF, 0xFF },       // white group 1
  { 0x00, 0x00, 0x00, 0xFF },       // black group 2
  { 0xFF, 0x66, 0x00, 0xFF },       // orange
  { 0x22, 0x22, 0xFF, 0xFF },       // medium blue
  { 0xFF, 0xFF, 0xFF, 0xFF }        // white group 2
};



enum {
  kZero,
  kEven,
  kOdd,
  kOne,
  kColorStateCount
};

//  HGR colors black, green/orange (odd), violet/blue (even), white
//    violet even ; green odd   (hcolor 2, 1)
//    orange even ; blue odd    (hcolor 5, 6)
uint8_t abgrFromHGRBitTable[kColorStateCount][2] = {
  { 0, 4 },   /* black */
  { 2, 6 },   /* even */
  { 1, 5 },   /* odd */
  { 3, 7 }    /* white */
};

//  bit 0 = incoming pixel at x+1, bit 1 = current pixel, bit 2 = pixel at x-1
unsigned stateToColorAction[8] = {
  /* b000 */  0,    //  > 2 adjacent off = black
  /* b001 */  0,    //  2 adjacent off outgoing  = black
  /* b010 */  1,    //  color at bit 1
  /* b011 */  3,    //  2 adjacent on incoming = white
  /* b100 */  0,    //  2 adhacent off incoming = black
  /* b101 */  2,    //  color at bit 2
  /* b110 */  3,    //  2 adjacent on outgoing = white
  /* b111 */  3,    //  > 2 adjacent on = white
};

void a2hgrToABGR8Scale2x1(uint8_t* pixout, const uint8_t* hgr){
  //  input is 40 bytes of hgr data to 280 bytes (1 byte per pixel)
  //  colors are one, zero, even, odd
  //  strategy:
  //
  //  two pixels: X and X + 1, and an extra 'register' BIT = zero
  //  1 and 1 = white; BIT = one
  //  1 and 0 = white if BIT = one else color(BIT) where BIT = even/odd(X)
  //  0 and 1 = black if BIT = zero else color(BIT) where BIT = even/odd(X + 1)
  //  0 and 0 = black; BIT = zero
  //
  //  0  1  2  3  4  5  6  7  8  9 10 11 12
  //  --------------------------------------
  //  0  0                                 |black; BIT = zero
  //     0  1                              |black if BIT = zero; BIT = even
  //        1  0                           |color(BIT) if BIT = zero; BIT = even
  //           0  1                        |color(BIT) if BIT = one; BIT = even
  //              1  1                     |white; BIT = one
  //                 1  0                  |white; if BIT = one; BIT = odd
  //                    0  1               |color(BIT); if BIT = odd; BIT = odd
  //                       1  0            |color(BIT); if BIT = odd; BIT = odd
  //                          0  0         |black; BIT = zero
  //
  //  "BIT" really is the value at X-1, so...
  //
  //  X-2, X-1, X
  //  any  1    1   = white
  //   1   1    0   = white
  //   0   1    0   = color(even/odd(X-1), group)
  //   0   0    1   = black
  //   1   0    1   = color(even/odd(X), group)
  //  any  0    0   = black
  //
  //  group = bit 7 of color/pixel byte

  unsigned state = 0;
  int xpos = -2;
  unsigned group;
  uint8_t pixel;
  for (int byteIdx = 0; byteIdx < 40; ++byteIdx) {
    uint8_t byte = hgr[byteIdx];
    group = byte >> 7;
    unsigned pxmask = 0x1;
    while (pxmask != 0x80) {
      unsigned bit = byte & pxmask;
      state <<= 1;
      pxmask <<= 1;
      if (bit) state |= 1;

      unsigned action = stateToColorAction[state & 0x7];
      unsigned color;
      if (xpos >= 0) {
        if (action == 0) color = 0;
        else if (action == 1) {
          if (xpos & 2) color = 2;
          else color = 1;
        } else if (action == 2) {
          if (xpos & 2) color = 1;
          else color = 2;
        } else {
          color = 3;
        }
        //  normalize hcolor 0 to 7 to 0-255 to be shader friendly
        pixel = (abgrFromHGRBitTable[color][group & 1] << 5) + 16;
        pixout[xpos] = pixel;
        pixout[xpos+1] = pixel;
      }
      xpos += 2;
    }
  }
  state <<= 1;
  unsigned action = stateToColorAction[state & 0x7];
  unsigned color;
  if (action == 0) color = 0;
  else if (action == 1) {
    color = 2;
  } else if (action == 2) {
    color = 1;
  } else {
    color = 3;
  }
  pixel = (abgrFromHGRBitTable[color][group & 1] << 5) + 16;
  pixout[xpos] = pixel;
  pixout[xpos+1] = pixel;
}


} // namespace



static sg_image loadFont(const char* pathname, stbtt_bakedchar* glyphSet)
{
//  TODO: move font load and setup into a shared class or static data
  FILE* fp = fopen(pathname, "rb");
  if (!fp) {
    return sg_image{};
  }
  fseek(fp, 0, SEEK_END);

  long sz = ftell(fp);
  unsigned char* buf = (unsigned char*)malloc(sz);
  fseek(fp, 0, SEEK_SET);
  fread(buf, 1, sz, fp);
  fclose(fp);

  unsigned char *textureData = (unsigned char *)malloc(
    kFontTextureWidth * kFontTextureHeight);

  stbtt_BakeFontBitmap(buf, 0, 16.0f,
                       textureData, kFontTextureWidth, kFontTextureHeight,
                       0xe000, 512,
                       glyphSet);

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

  free(buf);
  free(textureData);

  return fontImage;
}


ClemensDisplayProvider::ClemensDisplayProvider() {
  systemFontImage_ = loadFont("fonts/PrintChar21.ttf", kGlyphSet40col);
  systemFontImageHi_ = loadFont("fonts/PRNumber3.ttf", kGlyphSet80col);

  //  map screen byte code to glyph index
  //    a 32-bit word split into two 16-bit half-words.  half-word values will
  //    differ if the character code is flashing
  for (int i = 0; i < 0x20; ++i) {
    kPrimarySetToGlyph[i] =         (0x140 + i) | ((0x140 + i) << 16);
    kPrimarySetToGlyph[i + 0x20] =  (0x120 + i) | ((0x120 + i) << 16);
    kPrimarySetToGlyph[i + 0x40] =  (0x40 + i)  | ((0x140 + i) << 16);
    kPrimarySetToGlyph[i + 0x60] =  (0x20 + i)  | ((0x120 + i) << 16);
    kPrimarySetToGlyph[i + 0x80] =  (0x40 + i)  | ((0x40 + i) << 16);
    kPrimarySetToGlyph[i + 0xA0] =  (0x20 + i)  | ((0x20 + i) << 16);
    kPrimarySetToGlyph[i + 0xC0] =  (0x40 + i)  | ((0x40 + i) << 16);
    kPrimarySetToGlyph[i + 0xE0] =  (0x60 + i)  | ((0x60 + i) << 16);

    kAlternateSetToGlyph[i] =         (0x140 + i) | ((0x140 + i) << 16);
    kAlternateSetToGlyph[i + 0x20] =  (0x120 + i) | ((0x120 + i) << 16);
    kAlternateSetToGlyph[i + 0x40] =  (0x80 + i)  | ((0x80 + i) << 16);
    kAlternateSetToGlyph[i + 0x60] =  (0x60 + i)  | ((0x160 + i) << 16);
    kAlternateSetToGlyph[i + 0x80] =  (0x40 + i)  | ((0x40 + i) << 16);
    kAlternateSetToGlyph[i + 0xA0] =  (0x20 + i)  | ((0x20 + i) << 16);
    kAlternateSetToGlyph[i + 0xC0] =  (0x40 + i)  | ((0x40 + i) << 16);
    kAlternateSetToGlyph[i + 0xE0] =  (0x60 + i)  | ((0x60 + i) << 16);
  }


  //  create shader
  sg_shader_desc shaderDesc = {};
  shaderDesc.vs.uniform_blocks[0].size = sizeof(DisplayVertexParams);
  shaderDesc.attrs[0].sem_name = "POSITION";
  shaderDesc.attrs[1].sem_name = "TEXCOORD";
  shaderDesc.attrs[2].sem_name = "COLOR";
  shaderDesc.vs.source = VS_D3D11_VERTEX;
  shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
  shaderDesc.fs.source = FS_D3D11_TEXT;
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
  renderPipelineDesc.depth.pixel_format = SG_PIXELFORMAT_NONE;
  textPipeline_ = sg_make_pipeline(renderPipelineDesc);

  //  create hires pipeline and vertex buffer, no alpha blending, triangles
  shaderDesc = {};
  shaderDesc.vs.uniform_blocks[0].size = sizeof(DisplayVertexParams);
  shaderDesc.attrs[0].sem_name = "POSITION";
  shaderDesc.attrs[1].sem_name = "TEXCOORD";
  shaderDesc.attrs[2].sem_name = "COLOR";
  shaderDesc.vs.source = VS_D3D11_VERTEX;
  shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
  shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
  shaderDesc.fs.source = FS_D3D11_HIRES;
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
}

ClemensDisplayProvider::~ClemensDisplayProvider() {
  sg_destroy_pipeline(hiresPipeline_);
  sg_destroy_shader(hiresShader_);
  sg_destroy_pipeline(textPipeline_);
  sg_destroy_shader(textShader_);
  sg_destroy_image(systemFontImageHi_);
  sg_destroy_image(systemFontImage_);
}

ClemensDisplay::ClemensDisplay(ClemensDisplayProvider& provider) :
  provider_(provider)
{
  //  This data is specific to display and should be instanced per display
  sg_buffer_desc vertexBufDesc = { };
  vertexBufDesc.usage = SG_USAGE_STREAM;
  vertexBufDesc.size = (kDisplayTextRowLimit * kDisplayTextColumnLimit * 6) * (
    sizeof(DrawVertex));
  textVertexBuffer_ = sg_make_buffer(&vertexBufDesc);

  vertexBufDesc = { };
  vertexBufDesc.usage = SG_USAGE_STREAM;
  vertexBufDesc.size = 6 * sizeof(DrawVertex);
  vertexBuffer_ = sg_make_buffer(&vertexBufDesc);

  //  sokol doesn't support Texture1D out of the box, so fake it with a 2D
  //  abgr color texture of 8 vertical lines, 8 pixels high.
  uint8_t hiresColorData[32 * 8];
  for (int y = 0; y < 8; ++y) {
    uint8_t* texdata = &hiresColorData[32 * y];
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
  imageDesc.min_filter = SG_FILTER_LINEAR;
  imageDesc.mag_filter = SG_FILTER_LINEAR;
  imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.usage = SG_USAGE_IMMUTABLE;
  imageDesc.data.subimage[0][0].ptr = hiresColorData;
  imageDesc.data.subimage[0][0].size = sizeof(hiresColorData);
  colorArray_ = sg_make_image(imageDesc);

  imageDesc = {};
  imageDesc.width = kGraphicsTextureWidth;
  imageDesc.height = kGraphicsTextureHeight;
  imageDesc.pixel_format = SG_PIXELFORMAT_R8;
  imageDesc.min_filter = SG_FILTER_LINEAR;
  imageDesc.mag_filter = SG_FILTER_LINEAR;
  imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.usage = SG_USAGE_STREAM;
  graphicsTarget_ = sg_make_image(imageDesc);

  emulatorVideoBuffer_ = new uint8_t[kGraphicsTextureWidth * kGraphicsTextureHeight];

  //  create offscreen pass and image targets
  //const int rtSampleCount = sg_query_features().msaa_render_targets ?
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

ClemensDisplay::~ClemensDisplay()
{
  sg_destroy_pass(screenPass_);
  sg_destroy_image(screenTarget_);
  sg_destroy_image(graphicsTarget_);
  sg_destroy_image(colorArray_);
  sg_destroy_buffer(vertexBuffer_);
  sg_destroy_buffer(textVertexBuffer_);
}

void ClemensDisplay::start()
{
  sg_pass_action passAction = {};
  passAction.colors[0].action = SG_ACTION_CLEAR;
  passAction.colors[0].value = { 0.0f, 0.0f, 0.0f, 1.0f };

  sg_begin_pass(screenPass_, &passAction);
}

void ClemensDisplay::finish()
{
  sg_end_pass();
}


void ClemensDisplay::renderText40Col(
  const ClemensVideo& video,
  const uint8_t* mainMemory,
  bool useAlternateCharacterSet
) {
  renderTextPlane(video, 40, mainMemory, 0, useAlternateCharacterSet);
}

void ClemensDisplay::renderText80Col(
  const ClemensVideo& video,
  const uint8_t* mainMemory, const uint8_t* auxMemory,
  bool useAlternateCharacterSet
) {
  renderTextPlane(video, 80, auxMemory, 0, useAlternateCharacterSet);
  renderTextPlane(video, 80, mainMemory, 1, useAlternateCharacterSet);
}


void ClemensDisplay::renderTextPlane(
  const ClemensVideo& video,
  int columns,
  const uint8_t* memory,
  int phase,
  bool useAlternateCharacterSet
) {
  if (video.format != kClemensVideoFormat_Text &&
      video.format != kClemensVideoFormat_Text_Alternate
  ) {
    return;
  }

  const int kPhaseCount = columns / 40;
  float emulatorDisplayDims[2] = { 560, 384 };

  DisplayVertexParams vertexParams;
  vertexParams.virtual_dims[0] = columns;
  vertexParams.virtual_dims[1] = 24;
  vertexParams.display_ratio[0] = emulatorDisplayDims[0] / vertexParams.virtual_dims[0];
  vertexParams.display_ratio[1] = emulatorDisplayDims[1] / vertexParams.virtual_dims[1];
  vertexParams.render_dims[0] = kRenderTargetWidth;
  vertexParams.render_dims[1] = kRenderTargetHeight;

  stbtt_bakedchar* glyphSet;

  sg_bindings textBindings_ = {};
  textBindings_.vertex_buffers[0] = textVertexBuffer_;
  if (columns == 80) {
    textBindings_.fs_images[0] = provider_.systemFontImageHi_;
    glyphSet = kGlyphSet80col;
  } else {
    textBindings_.fs_images[0] = provider_.systemFontImage_;
    glyphSet = kGlyphSet40col;
  }

  sg_range rangeParam;
  rangeParam.ptr = &vertexParams;
  rangeParam.size = sizeof(vertexParams);

  DrawVertex vertices[40 * 6];
  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = video.scanline_byte_cnt * 6 * sizeof(DrawVertex);

  sg_apply_pipeline(provider_.textPipeline_);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

  for (int i = 0; i < video.scanline_count; ++i) {
    int row = i + video.scanline_start;
    const uint8_t* scanline = memory + video.scanlines[row].offset;
    auto* vertex = &vertices[0];
    for (int j = 0; j < video.scanline_byte_cnt; ++j) {
      //  TODO: handle flashing chars
      unsigned glyphIndex;
      if (useAlternateCharacterSet) {
        glyphIndex = kAlternateSetToGlyph[scanline[j]];
      } else {
        glyphIndex =  kPrimarySetToGlyph[scanline[j]];
      }
      glyphIndex &= 0xffff;
      auto* glyph = &glyphSet[glyphIndex];
      stbtt_aligned_quad quad;
      float xpos = ((j * kPhaseCount) + phase) * vertexParams.display_ratio[0];
      float ypos = row * vertexParams.display_ratio[1] + (vertexParams.display_ratio[1] - 1);
      stbtt_GetBakedQuad(glyphSet, kFontTextureWidth, kFontTextureHeight, glyphIndex, &xpos, &ypos, &quad, 1);
      float l = quad.x0 / vertexParams.display_ratio[0];
      float r = quad.x1 / vertexParams.display_ratio[0];
      float t = quad.y0 / vertexParams.display_ratio[1];
      float b = quad.y1 / vertexParams.display_ratio[1];
      vertex[0] = { { l, t }, { quad.s0, quad.t0 }, 0xffffffff };
      vertex[1] = { { l, b }, { quad.s0, quad.t1 }, 0xffffffff };
      vertex[2] = { { r, b }, { quad.s1, quad.t1 }, 0xffffffff };
      vertex[3] = { { l, t }, { quad.s0, quad.t0 }, 0xffffffff };
      vertex[4] = { { r, b }, { quad.s1, quad.t1 }, 0xffffffff };
      vertex[5] = { { r, t }, { quad.s1, quad.t0 }, 0xffffffff };
      vertex += 6;
    }
    auto vbOffset = sg_append_buffer(textBindings_.vertex_buffers[0], verticesRange);
    textBindings_.vertex_buffer_offsets[0] = vbOffset;
    sg_apply_bindings(textBindings_);
    sg_draw(0, 6 * video.scanline_byte_cnt, 1);
  }
}

void ClemensDisplay::renderHiresGraphics(
  const ClemensVideo& video,
  const uint8_t* memory
) {
  if (video.format != kClemensVideoFormat_Hires) {
    return;
  }

  const float emulatorDisplayDims[2] = { 560, 384 };

  //  TODO: simplify vertex shader for graphics screens
  //        a lot of these uniforms don't seem  necessary, but we have to set
  //        them up so the shader works.
  DisplayVertexParams vertexParams;
  vertexParams.virtual_dims[0] = 280;
  vertexParams.virtual_dims[1] = 192;
  vertexParams.display_ratio[0] = 2;
  vertexParams.display_ratio[1] = 2;
  vertexParams.render_dims[0] = kRenderTargetWidth;
  vertexParams.render_dims[1] = kRenderTargetHeight;

  //
  for (int i = 0; i < video.scanline_count; ++i) {
    int row = i + video.scanline_start;
    const uint8_t* scanline = memory + video.scanlines[row].offset;
    uint8_t* pixout = emulatorVideoBuffer_ + i * 2 * kGraphicsTextureWidth;
    a2hgrToABGR8Scale2x1(pixout, scanline);
    a2hgrToABGR8Scale2x1(pixout + kGraphicsTextureWidth, scanline);
  }

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
  float x1 = vertexParams.virtual_dims[0];
  float y1 = video.scanline_count;
  float u1 = (vertexParams.virtual_dims[0] * vertexParams.display_ratio[0]) / kGraphicsTextureWidth;
  float v1 = (video.scanline_count * vertexParams.display_ratio[1]) / kGraphicsTextureHeight;

  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = 6 * sizeof(DrawVertex);
  vertices[0] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[1] = { { 0.0f, y1   }, { 0.0f, v1   }, 0xffffffff };
  vertices[2] = { { x1,   y1   }, { u1,   v1   }, 0xffffffff };
  vertices[3] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[4] = { { x1,   y1   }, { u1,   v1   }, 0xffffffff };
  vertices[5] = { { x1,   0.0f }, { u1,  0.0f  }, 0xffffffff };

  sg_bindings renderBindings = {};
  renderBindings.vertex_buffers[0] = vertexBuffer_;
  renderBindings.fs_images[0] = graphicsTarget_;
  renderBindings.fs_images[1] = colorArray_;
  renderBindings.vertex_buffer_offsets[0] = (
    sg_append_buffer(renderBindings.vertex_buffers[0], verticesRange));
  sg_apply_bindings(renderBindings);
  sg_draw(0, 6, 1);
}
