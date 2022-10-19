#include "clem_display.hpp"
#include "cinek/buffer.hpp"

#include "render.h"


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
static const uint8_t kHiresColors[8][4] = {
  { 0x00, 0x00, 0x00, 0xFF },       // black group 1
  { 0x11, 0xDD, 0x00, 0xFF },       // green (light green)
  { 0xDD, 0x22, 0xDD, 0xFF },       // purple
  { 0xFF, 0xFF, 0xFF, 0xFF },       // white group 1
  { 0x00, 0x00, 0x00, 0xFF },       // black group 2
  { 0xFF, 0x66, 0x00, 0xFF },       // orange
  { 0x22, 0x22, 0xFF, 0xFF },       // medium blue
  { 0xFF, 0xFF, 0xFF, 0xFF }        // white group 2
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
  {   0,    0,    0,    255 },      // black
  { 221,    0,   51,    255 },      // deep red
  { 136,   85,    0,    255 },      // brown
  { 255,  102,    0,    255 },      // orange
  {   0,  119,   34,    255 },      // dark green
  {  85,   85,   85,    255 },      // dark gray
  {  17,  221,    0,    255 },      // lt. green
  { 255,  255,    0,    255 },      // yellow
  {   0,    0,  153,    255 },      // dark blue
  { 221,   34,  221,    255 },      // purple
  { 170,  170,  170,    255 },      // lt. gray
  { 255,  153,  136,    255 },      // pink
  {  34,   34,  255,    255 },      // med blue
  { 102,  170,  255,    255 },      // light blue
  {  68,  255,  153,    255 },      // aquamarine
  { 255,  255,  255,    255 }       // white
};

static const uint8_t kGr16Colors[16][4] = {
  {   0,    0,    0,    255 },      // black
  { 221,    0,   51,    255 },      // deep red
  {   0,    0,  153,    255 },      // dark blue
  { 221,   34,  221,    255 },      // purple
  {   0,  119,   34,    255 },      // dark green
  {  85,   85,   85,    255 },      // dark gray
  {  34,   34,  255,    255 },      // med blue
  { 102,  170,  255,    255 },      // light blue
  { 136,   85,    0,    255 },      // brown
  { 255,  102,    0,    255 },      // orange
  { 170,  170,  170,    255 },      // lt. gray
  { 255,  153,  136,    255 },      // pink
  {  17,  221,    0,    255 },      // lt. green
  { 255,  255,    0,    255 },      // yellow
  {  68,  255,  153,    255 },      // aquamarine
  { 255,  255,  255,    255 }       // white
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

void a2hgrToABGR8Scale2x2(uint8_t* pixout, uint8_t* pixout2, const uint8_t* hgr) {
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
        pixout2[xpos] = pixel;
        pixout2[xpos+1] = pixel;
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
  pixout2[xpos] = pixel;
  pixout2[xpos+1] = pixel;
}

uint32_t grColorToABGR(unsigned color)
{
  const uint8_t* grColor = &kGr16Colors[color][0];
  unsigned abgr = ((unsigned)grColor[3] << 24) |
                  ((unsigned)grColor[2] << 16) |
                  ((unsigned)grColor[1] << 8) |
                  grColor[0];
  return abgr;
} // namespace



static sg_image loadFont(stbtt_bakedchar* glyphSet,
                         const cinek::ByteBuffer& fileBuffer)
{
  unsigned char *textureData = (unsigned char *)malloc(
    kFontTextureWidth * kFontTextureHeight);

  stbtt_BakeFontBitmap(fileBuffer.getHead(), 0, 16.0f,
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

  free(textureData);

  return fontImage;
}

} // namespace anon

ClemensDisplayProvider::ClemensDisplayProvider(
  const cinek::ByteBuffer& systemFontLoBuffer,
  const cinek::ByteBuffer& systemFontHiBuffer
) {
  systemFontImage_ = loadFont(kGlyphSet40col, systemFontLoBuffer);
  systemFontImageHi_ = loadFont(kGlyphSet80col, systemFontHiBuffer);

  const uint8_t blankImageData[16] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };
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
  renderPipelineDesc.colors[0].blend.enabled = true;
  renderPipelineDesc.colors[0].write_mask = SG_COLORMASK_RGB;
  renderPipelineDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
  renderPipelineDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
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

  //  create hires pipeline and vertex buffer, no alpha blending, triangles
  shaderDesc = {};
  shaderDesc.vs.uniform_blocks[0].size = sizeof(DisplayVertexParams);
  shaderDesc.attrs[0].sem_name = "POSITION";
  shaderDesc.attrs[1].sem_name = "TEXCOORD";
  shaderDesc.attrs[2].sem_name = "COLOR";
  shaderDesc.vs.source = VS_D3D11_VERTEX;
  shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
  shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
  shaderDesc.fs.source = FS_D3D11_SUPER;
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

  if (systemFontFileBuffer_.getHead()) {
    free(systemFontFileBuffer_.getHead());
  }
  if (systemFontFileHiBuffer_.getHead()) {
    free(systemFontFileHiBuffer_.getHead());
  }
}

ClemensDisplay::ClemensDisplay(ClemensDisplayProvider& provider) :
  provider_(provider)
{
  //  This data is specific to display and should be instanced per display
  //  require double the vertices to display the background under the
  //  foreground
  sg_buffer_desc vertexBufDesc = { };
  vertexBufDesc.usage = SG_USAGE_STREAM;
  //  using 2 x 2 x 40x24 quads (lores has two pixels per box)
  vertexBufDesc.size = 4 * (kDisplayTextRowLimit * kDisplayTextColumnLimit * 6) * (
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
  hgrColorArray_ = sg_make_image(imageDesc);

  uint8_t dblHiresColorData[64 * 8];
  for (int y = 0; y < 8; ++y) {
    uint8_t* texdata = &dblHiresColorData[64 * y];
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
  imageDesc.min_filter = SG_FILTER_LINEAR;
  imageDesc.mag_filter = SG_FILTER_LINEAR;
  imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.usage = SG_USAGE_IMMUTABLE;
  imageDesc.data.subimage[0][0].ptr = dblHiresColorData;
  imageDesc.data.subimage[0][0].size = sizeof(dblHiresColorData);
  dblhgrColorArray_ = sg_make_image(imageDesc);

  emulatorRGBABuffer_ = new uint8_t[1024 * 8];

  imageDesc = {};
  imageDesc.width = 256;
  imageDesc.height = 8;
  imageDesc.type = SG_IMAGETYPE_2D;
  imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
  imageDesc.min_filter = SG_FILTER_LINEAR;
  imageDesc.mag_filter = SG_FILTER_LINEAR;
  imageDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  imageDesc.usage = SG_USAGE_STREAM;
  rgbaColorArray_ = sg_make_image(imageDesc);

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
  sg_destroy_image(hgrColorArray_);
  sg_destroy_image(dblhgrColorArray_);
  sg_destroy_image(rgbaColorArray_);
  sg_destroy_buffer(vertexBuffer_);
  sg_destroy_buffer(textVertexBuffer_);
  delete[] emulatorVideoBuffer_;
  delete[] emulatorRGBABuffer_;
}

void ClemensDisplay::start(
  const ClemensMonitor& monitor,
  int screen_w,
  int screen_h
) {
  sg_pass_action passAction = {};
  passAction.colors[0].action = SG_ACTION_CLEAR;
  const uint8_t* borderColor = &kGr16Colors[monitor.border_color & 0xf][0];
  passAction.colors[0].value = {
    borderColor[0]/255.0f,
    borderColor[1]/255.0f,
    borderColor[2]/255.0f,
    1.0f
  };

  sg_begin_pass(screenPass_, &passAction);

  emulatorMonitorDimensions_[0] = screen_w;
  emulatorMonitorDimensions_[1] = screen_h;

  emulatorVideoDimensions_[0] = monitor.width;
  emulatorVideoDimensions_[1] = monitor.height;

  emulatorTextColor_ = monitor.text_color;
}

void ClemensDisplay::finish(float *uvs)
{
  sg_end_pass();
  uvs[0] = emulatorMonitorDimensions_[0] / kRenderTargetWidth;
  uvs[1] = emulatorMonitorDimensions_[1] / kRenderTargetHeight;
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
  if (video.format != kClemensVideoFormat_Text) {
    return;
  }

  const int kPhaseCount = columns / 40;

  auto vertexParams = createVertexParams(columns, 24);

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

  sg_bindings backBindings = {};
  backBindings.vertex_buffers[0] = textVertexBuffer_;
  backBindings.fs_images[0] = provider_.blankImage_;

  DrawVertex vertices[40 * 6];
  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = video.scanline_byte_cnt * 6 * sizeof(DrawVertex);

  sg_range rangeParam;
  rangeParam.ptr = &vertexParams;
  rangeParam.size = sizeof(vertexParams);
  sg_apply_pipeline(provider_.textPipeline_);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

  // pass 1 - background
  unsigned textABGR = grColorToABGR((emulatorTextColor_ >> 4) & 0xf);

  for (int i = 0; i < video.scanline_count; ++i) {
    int row = i + video.scanline_start;
    const uint8_t* scanline = memory + video.scanlines[row].offset;
    auto* vertex = &vertices[0];
    for (int j = 0; j < video.scanline_byte_cnt; ++j) {
      float x0 = ((j * kPhaseCount) + phase);
      float y0 = i;
      float x1 = x0 + 1.0f;
      float y1 = y0 + 1.0f;
      vertex[0] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[1] = { { x0, y1 }, { 0.0f, 1.0f }, textABGR };
      vertex[2] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[3] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[4] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[5] = { { x1, y0 }, { 1.0f, 0.0f }, textABGR };
      vertex += 6;
    }
    auto vbOffset = sg_append_buffer(backBindings.vertex_buffers[0], verticesRange);
    backBindings.vertex_buffer_offsets[0] = vbOffset;
    sg_apply_bindings(backBindings);
    sg_draw(0, 6 * video.scanline_byte_cnt, 1);
  }

  //  poss 2 - foreground
  textABGR = grColorToABGR(emulatorTextColor_ & 0xf);
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
      vertex[0] = { { l, t }, { quad.s0, quad.t0 }, textABGR };
      vertex[1] = { { l, b }, { quad.s0, quad.t1 }, textABGR };
      vertex[2] = { { r, b }, { quad.s1, quad.t1 }, textABGR };
      vertex[3] = { { l, t }, { quad.s0, quad.t0 }, textABGR };
      vertex[4] = { { r, b }, { quad.s1, quad.t1 }, textABGR };
      vertex[5] = { { r, t }, { quad.s1, quad.t0 }, textABGR };
      vertex += 6;
    }
    auto vbOffset = sg_append_buffer(textBindings_.vertex_buffers[0], verticesRange);
    textBindings_.vertex_buffer_offsets[0] = vbOffset;
    sg_apply_bindings(textBindings_);
    sg_draw(0, 6 * video.scanline_byte_cnt, 1);
  }
}

void ClemensDisplay::renderLoresGraphics(
  const ClemensVideo& video,
  const uint8_t* memory
) {
  renderLoresPlane(video, 40, memory, 0);
}

void ClemensDisplay::renderLoresPlane(
  const ClemensVideo& video,
  int columns, const uint8_t* memory,
  int phase
) {
  if (video.format != kClemensVideoFormat_Lores) {
    return;
  }

  const int kPhaseCount = columns / 40;

  auto vertexParams = createVertexParams(columns, 48);
  sg_bindings pixelBindings_ = {};
  pixelBindings_.vertex_buffers[0] = textVertexBuffer_;
  pixelBindings_.fs_images[0] = provider_.blankImage_;

  DrawVertex vertices[80 * 6];
  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = 2 * video.scanline_byte_cnt * 6 * sizeof(DrawVertex);

  sg_range rangeParam;
  rangeParam.ptr = &vertexParams;
  rangeParam.size = sizeof(vertexParams);
  sg_apply_pipeline(provider_.textPipeline_);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

  // pass 1 - background
  for (int i = 0; i < video.scanline_count; ++i) {
    int row = i + video.scanline_start;
    const uint8_t* scanline = memory + video.scanlines[row].offset;
    auto* vertex = &vertices[0];
    for (int j = 0; j < video.scanline_byte_cnt; ++j) {
      float x0 = ((j * kPhaseCount) + phase);
      float y0 = i * 2;
      float x1 = x0 + 1.0f;
      float y1 = y0 + 1.0f;

      uint8_t block = scanline[j];
      unsigned textABGR = grColorToABGR(block & 0xf);
      vertex[0] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[1] = { { x0, y1 }, { 0.0f, 1.0f }, textABGR };
      vertex[2] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[3] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[4] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[5] = { { x1, y0 }, { 1.0f, 0.0f }, textABGR };
      vertex += 6;
      textABGR = grColorToABGR(block >> 4);
      y0 = y1;
      y1 += 1.0f;
      vertex[0] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[1] = { { x0, y1 }, { 0.0f, 1.0f }, textABGR };
      vertex[2] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[3] = { { x0, y0 }, { 0.0f, 0.0f }, textABGR };
      vertex[4] = { { x1, y1 }, { 1.0f, 1.0f }, textABGR };
      vertex[5] = { { x1, y0 }, { 1.0f, 0.0f }, textABGR };
      vertex += 6;
    }
    auto vbOffset = sg_append_buffer(pixelBindings_.vertex_buffers[0], verticesRange);
    pixelBindings_.vertex_buffer_offsets[0] = vbOffset;
    sg_apply_bindings(pixelBindings_);
    sg_draw(0, 12 * video.scanline_byte_cnt, 1);
  }
}

void ClemensDisplay::renderHiresGraphics(
  const ClemensVideo& video,
  const uint8_t* memory
) {
  if (video.format != kClemensVideoFormat_Hires) {
    return;
  }

  //  TODO: simplify vertex shader for graphics screens
  //        a lot of these uniforms don't seem  necessary, but we have to set
  //        them up so the shader works.
  auto vertexParams = createVertexParams(
    emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);
  //  draw the graphics data with the incredible A2 hires color rules in mind
  //  and scale in software the pixels to 2x2 so they conform to our output
  //  texture size (which is 4x the size of a 280x192 screen)
  for (int i = 0; i < video.scanline_count; ++i) {
    int row = i + video.scanline_start;
    const uint8_t* scanline = memory + video.scanlines[row].offset;
    uint8_t* pixout = emulatorVideoBuffer_ + i * 2 * kGraphicsTextureWidth;
    a2hgrToABGR8Scale2x2(pixout, pixout + kGraphicsTextureWidth, scanline);
  }

  renderHiresGraphicsTexture(video, vertexParams, hgrColorArray_);
}

//  Interesting...
//    References: Patent - US4786893A
//    "Method and apparatus for generating RGB color signals from composite
//      digital video signal"
//
//    https://patents.google.com/patent/US4786893A/en?oq=US4786893
//
//    Patent seems to refer to Apple II composite signals converted to RGB
//    using the sliding bit window as referred to in the Hardware Reference.
//    It's likely this method is used in the IIgs - and so it's good enough for
//    a baseline (doesn't fix IIgs Double Hires issues related to artifacting
//    that allows better quality for NTSC hardware... which is another issue)
//
//  Notes:
//    The "Prior Art Method" described in the patent matches my first naive
//    implementation (4 bits per effective pixel = the color.)  The problem
//    with this is that data is streamed serially to the controller vs on a per
//    nibble basis.  This becomes an issue when transitioning between colors
//    and the 4-bit color isn't aligned on the nibble.
//
//  Concept:
//    Implement a version of the "Present Invention" from the patent
//    - Given the most recent bit from the bitstream
//    - if the result indicates a color pattern change, then render the
//      original color until the color pattern change occurs
//
//  Details:
//    Bit stream: incoming from pixin
//    Shift register:
//        history (most recent 4 bits being relevant)
//    Barrel shifter:
//        the original color at the start of the 4-bit string
//        (this can be simplified in software as just a stored-off value)
//    Color Change Test:
//        If Shift Register Bit 3 != incoming bit, then color change
//    Plot:
//        If Color Change, Select Latch Color
//        Else Select Barrel Shifted Color (Current)
//        Set Latch color to selected color
//    Latched Color:
//        Initially Zero
//
//  This is a literal translation of the patent's Fig. 4 - which works pretty
//  well to emulate the IIgs implementation.   This could be optimized via
//  lookup tables.
//
//  TODO: optimize using lookup tables
//
static inline bool jk_ff(bool j, bool k, bool q) {
  if (!j && !k) return q;
  if (!j && k) return 0;
  if (j && !k) return 1;
  return !q;
}

void a2dhgrToABGR81x2(
  uint8_t* pixout0, uint8_t* pixout1,
  const uint8_t* scanlines[2],
  int scanlineByteCnt
) {
  int pixinByteCounter = 0;
  int pixinBitCounter = 0;
  uint8_t pixinByte = *scanlines[0];
  uint8_t shifter = 0;
  uint8_t barrel = 0;
  uint8_t latch = 0;
  bool jk0 = false;
  bool jk1 = false;

  scanlineByteCnt <<= 1;
  while (pixinByteCounter < scanlineByteCnt) {
    bool pixinBit = (pixinByte & 0x1);
    bool colorChanged0 = (pixinBit && !(shifter & 0x8));
    bool colorChanged1 = (!pixinBit && (shifter & 0x8));
    unsigned barrelShift = (pixinBitCounter % 4);
    barrel = shifter >> barrelShift;
    barrel |= (shifter << (4 - barrelShift));
    barrel &= 0xf;

    uint8_t selected =  (jk0 || jk1) ? latch : barrel;
    uint8_t pixout = (latch << 4) + 8;
    *(pixout0++) = pixout;
    *(pixout1++) = pixout;

    //  next clock
    jk0 = jk_ff(colorChanged0, shifter & 0x4, jk0);
    jk1 = jk_ff(colorChanged1, !(shifter & 0x4), jk1);
    shifter <<= 1;
    shifter |= (pixinBit ? 0x1 : 0);
    latch = selected;
    pixinByte >>= 1;
    ++pixinBitCounter;
    if (!(pixinBitCounter % 7)) {
      ++scanlines[pixinByteCounter % 2];
      ++pixinByteCounter;
      pixinByte = *scanlines[pixinByteCounter % 2];
    }
  }
}

void ClemensDisplay::renderDoubleHiresGraphics(
  const ClemensVideo& video,
  const uint8_t* main,
  const uint8_t* aux
) {
  if (video.format != kClemensVideoFormat_Double_Hires) {
    return;
  }
  auto vertexParams = createVertexParams(
    emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);


  //  An oversimplication of double hires reads that the 'effective' resolution
  //  is 4 pixels per color (so 140x192 - let's say a color is a 'block' of 4
  //  pixels.   Since a block is a 4-bit pattern representing actual pixels on
  //  the screen, adjacent blocks to the current block of interest will effect
  //  this block.   To best handle the 'bit per pixel' method of rendering,
  //  where the pixel color is determine by past state, our plotter will
  //  'slide' along the bit array.  At some point the plotter will decide what
  //  color to render at an earlier point in the array and proceed ahead.
  //
  for (int y = 0; y < video.scanline_count; ++y) {
    int row = y + video.scanline_start;
    const uint8_t* pixsources[2] = {
      aux + video.scanlines[row].offset,
      main + video.scanlines[row].offset
    };
    uint8_t* pixout = emulatorVideoBuffer_ + y * 2 * kGraphicsTextureWidth;
    a2dhgrToABGR81x2(pixout, pixout + kGraphicsTextureWidth, pixsources, video.scanline_byte_cnt);
    /*
    const uint8_t* pixin = pixsources[0];
    unsigned color = 0;
    unsigned x = 0, xi = 0;
    uint8_t data = pixin[0];
    while (x < 560) {
      color <<= 1;
      if (data & 0x1) color |= 0x1;
      data >>= 1;
      ++x;
      if ((x % 4) == 0) {
        unsigned xo = x - 4;
        for (unsigned xo = x - 4; xo < x; ++xo) {
          unsigned pixel = ((color & 0xf) << 4) + 8;
          pixout[xo] = pixel;
          (pixout + kGraphicsTextureWidth)[xo] = pixel;
        }
      }
      if ((x % 7) == 0) {
        ++xi;
        pixin = pixsources[xi % 2];
        data = pixin[xi >> 1];
      }
    }
    */
  }
  //
  renderHiresGraphicsTexture(video, vertexParams, dblhgrColorArray_);
}

void ClemensDisplay::renderHiresGraphicsTexture(
  const ClemensVideo& video,
  const DisplayVertexParams& vertexParams,
  sg_image colorArray
) {
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
  float u1 = emulatorVideoDimensions_[0] / kGraphicsTextureWidth;
  float v1 = (video.scanline_count * y_scalar) / kGraphicsTextureHeight;

  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = 6 * sizeof(DrawVertex);
  vertices[0] = { { x0, y0 }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[1] = { { x0, y1 }, { 0.0f, v1   }, 0xffffffff };
  vertices[2] = { { x1, y1 }, { u1,   v1   }, 0xffffffff };
  vertices[3] = { { x0, y0 }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[4] = { { x1, y1 }, { u1,   v1   }, 0xffffffff };
  vertices[5] = { { x1, y0 }, { u1,  0.0f  }, 0xffffffff };

  sg_bindings renderBindings = {};
  renderBindings.vertex_buffers[0] = vertexBuffer_;
  renderBindings.fs_images[0] = graphicsTarget_;
  renderBindings.fs_images[1] = colorArray;
  renderBindings.vertex_buffer_offsets[0] = (
    sg_append_buffer(renderBindings.vertex_buffers[0], verticesRange));
  sg_apply_bindings(renderBindings);
  sg_draw(0, 6, 1);
}

void ClemensDisplay::renderSuperHiresGraphics(
  const ClemensVideo& video,
  const uint8_t* memory
) {
  // 1x2 pixels
  uint8_t* video_out = emulatorVideoBuffer_;
  clemens_render_video(&video, memory, video_out,
                       kGraphicsTextureWidth, kGraphicsTextureHeight,
                       kGraphicsTextureWidth * 2, 1);


  uint8_t* buffer0 = video_out;
  for (unsigned y = 0; y < video.scanline_count; ++y) {
    uint8_t* buffer1 = buffer0 + kGraphicsTextureWidth;
    memcpy(buffer1, buffer0, kGraphicsTextureWidth);
    buffer0 += kGraphicsTextureWidth * 2;
  }

  for (unsigned y = 0; y < 8; ++y) {
    uint8_t* texdata = &emulatorRGBABuffer_[1024 * y];
    for (unsigned x = 0; x < 256; ++x) {
        texdata[x * 4] = (uint8_t)(video.rgba[x] >> 24);
        texdata[x * 4 + 1] = (uint8_t)((video.rgba[x] >> 16) & 0xff);
        texdata[x * 4 + 2] = (uint8_t)((video.rgba[x] >> 8) & 0xff);
        texdata[x * 4 + 3] = (uint8_t)(video.rgba[x] & 0xff);
    }
  }

  sg_image_data graphicsImageData = {};
  graphicsImageData.subimage[0][0].ptr = emulatorVideoBuffer_;
  graphicsImageData.subimage[0][0].size = kGraphicsTextureWidth * kGraphicsTextureHeight;
  sg_update_image(graphicsTarget_, graphicsImageData);

  graphicsImageData.subimage[0][0].ptr = emulatorRGBABuffer_;
  graphicsImageData.subimage[0][0].size = 256 * 4 * 8;
  sg_update_image(rgbaColorArray_, graphicsImageData);

  auto vertexParams = createVertexParams(
    emulatorVideoDimensions_[0], emulatorVideoDimensions_[1]);
  sg_range rangeParam;
  rangeParam.ptr = &vertexParams;
  rangeParam.size = sizeof(vertexParams);

  sg_apply_pipeline(provider_.superHiresPipeline_);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, rangeParam);

  //  texture contains a scaled version of the original 280 x 160/192 screen
  //  to avoid UV rounding issues
  DrawVertex vertices[6];
  float y_scalar = emulatorVideoDimensions_[1] / 200.0f;
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = x0 + emulatorVideoDimensions_[0];
  float y1 = y0 + (video.scanline_count * y_scalar);
  float u1 = emulatorVideoDimensions_[0] / kGraphicsTextureWidth;
  float v1 = (video.scanline_count * y_scalar) / kGraphicsTextureHeight;

  sg_range verticesRange;
  verticesRange.ptr = &vertices[0];
  verticesRange.size = 6 * sizeof(DrawVertex);
  vertices[0] = { { x0, y0 }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[1] = { { x0, y1 }, { 0.0f, v1   }, 0xffffffff };
  vertices[2] = { { x1, y1 }, { u1,   v1   }, 0xffffffff };
  vertices[3] = { { x0, y0 }, { 0.0f, 0.0f }, 0xffffffff };
  vertices[4] = { { x1, y1 }, { u1,   v1   }, 0xffffffff };
  vertices[5] = { { x1, y0 }, { u1,  0.0f  }, 0xffffffff };

  sg_bindings renderBindings = {};
  renderBindings.vertex_buffers[0] = vertexBuffer_;
  renderBindings.fs_images[0] = graphicsTarget_;
  renderBindings.fs_images[1] = rgbaColorArray_;
  renderBindings.vertex_buffer_offsets[0] = (
    sg_append_buffer(renderBindings.vertex_buffers[0], verticesRange));
  sg_apply_bindings(renderBindings);
  sg_draw(0, 6, 1);
}


auto ClemensDisplay::createVertexParams(
  float virtualDimX,
  float virtualDimY
) -> DisplayVertexParams
{
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
