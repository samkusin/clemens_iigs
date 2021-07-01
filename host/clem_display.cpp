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
  auto& uniformBlock = shaderDesc.vs.uniform_blocks[0];
  uniformBlock.size = sizeof(DisplayVertexParams);
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
}

ClemensDisplayProvider::~ClemensDisplayProvider() {
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

  //  create offscreen pass and image targets
  //const int rtSampleCount = sg_query_features().msaa_render_targets ?
  sg_image_desc renderTargetDesc = {};
  renderTargetDesc.render_target = true;
  renderTargetDesc.width = 1024;
  renderTargetDesc.height = 512;
  renderTargetDesc.min_filter = SG_FILTER_LINEAR;
  renderTargetDesc.mag_filter = SG_FILTER_LINEAR;
  renderTargetDesc.sample_count = 1;
  screenTarget_ = sg_make_image(renderTargetDesc);

  sg_pass_desc passDesc = {};
  passDesc.color_attachments[0].image = screenTarget_;
  screenPass_ = sg_make_pass(passDesc);
}

ClemensDisplay::~ClemensDisplay()
{
  sg_destroy_pass(screenPass_);
  sg_destroy_image(screenTarget_);
  sg_destroy_buffer(textVertexBuffer_);
}


void ClemensDisplay::renderText40Col(
  const ClemensVideo& video,
  const uint8_t* mainMemory,
  bool useAlternateCharacterSet
) {
  sg_pass_action passAction = {};
  passAction.colors[0].action = SG_ACTION_CLEAR;
  passAction.colors[0].value = { 0.0f, 0.0f, 0.0f, 1.0f };
  sg_begin_pass(screenPass_, &passAction);
  renderTextPlane(video, 40, mainMemory, 0, useAlternateCharacterSet);
  sg_end_pass();
}

void ClemensDisplay::renderText80Col(
  const ClemensVideo& video,
  const uint8_t* mainMemory, const uint8_t* auxMemory,
  bool useAlternateCharacterSet
) {
  sg_pass_action passAction = {};
  passAction.colors[0].action = SG_ACTION_CLEAR;
  passAction.colors[0].value = { 0.0f, 0.0f, 0.0f, 1.0f };
  sg_begin_pass(screenPass_, &passAction);
  renderTextPlane(video, 80, auxMemory, 0, useAlternateCharacterSet);
  renderTextPlane(video, 80, mainMemory, 1, useAlternateCharacterSet);
  sg_end_pass();
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
  vertexParams.render_dims[0] = 1024;
  vertexParams.render_dims[1] = 512;

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

  sg_pass_action passAction = {};
  passAction.colors[0].action = SG_ACTION_CLEAR;
  passAction.colors[0].value = { 0.0f, 0.0f, 0.0f, 1.0f };

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
