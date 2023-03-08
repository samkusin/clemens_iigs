// Note all textures are guaranteed to have power of 2 width and heights

const char *VS_VERTEX_SOURCE = "#version 330\n"
                               "uniform vec2 render_dims;\n"
                               "uniform vec2 display_ratio;\n"
                               "uniform vec2 virtual_dims;\n"
                               "uniform vec2 offsets;\n"
                               "\n"
                               "layout(location = 0) in vec2 pos;\n"
                               "layout(location = 1) in vec2 uv1;\n"
                               "layout(location = 2) in vec4 color1;\n"
                               "out vec2 uv;\n"
                               "out vec4 color;\n"
                               "void main() {\n"
                               "  vec2 t_pos = (pos * display_ratio + offsets) / render_dims;\n"
                               "  t_pos = (t_pos - 0.5) * vec2(2.0, -2.0);\n"
                               "  gl_Position = vec4(t_pos, 0.5, 1.0);\n"
                               "  uv = uv1;\n"
                               "  color = color1;\n"
                               "}\n";

const char *FS_TEXT_SOURCE = "#version 330\n"
                             "uniform sampler2D tex;\n"
                             "in vec4 color;\n"
                             "in vec2 uv;\n"
                             "out vec4 frag_color;\n"
                             "void main() {\n"
                             "  frag_color = texture(tex, uv).xxxx * color;\n"
                             "}\n";

const char *FS_HIRES_SOURCE = "#version 330\n"
                              "uniform sampler2D hgr_tex;\n"
                              "uniform sampler2D hcolor_tex;\n"
                              "in vec4 color;\n"
                              "in vec2 uv;\n"
                              "out vec4 frag_color;\n"
                              "void main() {\n"
                              "  vec4 texl_hgr = texture(hgr_tex, uv);\n"
                              "  float cx = texl_hgr.x;\n"
                              "  frag_color = texture(hcolor_tex, vec2(cx, 0.0));\n"
                              "}\n";

/*
  <color_tex> contains a matrix of 16 colors per scanline.  Each color is
    a 4x4 texel to account for bleedover

  <screen_tex> is the common 1024 x 512 target that contains *all* of the
    screen modes.
    - for super-hires, each pixel is a 1x2 texel
    - 320 mode duplicates the pixel horizontally on the CPU side

  <screen_params> contains a few useful values
    - <x> screen_tex width
    - <y> screen_tex height
    - <z> screen_texel_width (likely 1)
    - <w> screen_texel_height (likely 2)

  <color_params> contains a few useful values
    - <x> color_tex width
    - <y> color_tex height
    - <z> color texel width (likely 4)
    - <w> color_texel height (likely 4)

  To support palette switching (for > 256 colors per frame) the `color_tex`
  as described above is a matrix of 16 colors horizontally and 200 of them
  vertically (4x4 actual pixels on the texture)

  <uv> applies to the `screen_tex`.
  <color> applies to the `screen_tex` and is a value from ((0 - 255) + 8) / 16.

  foreach screen_tex pixel with (U, V) and COLOR:
      color_index = floor((sample(screen_tex, UV) * 255.0 / 16.0) + 0.5)
      scanline = floor(((1.0 - Vs) * screen_params.w / screen_params.y) + 0.5)
      color_uv = (
        (color_index + 0.5) / 16.0f,
        1.0 - ((scanline + 0.5) * color_params.w) / color_tex.y
      )
      frag_color = sample(color_tex, color_uv) * COLOR
*/

const char *FS_SUPER_SOURCE = R"glsl(
#version 330

uniform sampler2D screen_tex;
uniform sampler2D color_tex;
uniform vec4 screen_params;
uniform vec4 color_params;

in vec4 color;
in vec2 uv;
out vec4 frag_color;

void main() {
    //  color_index = 0 to 15
    //  scanline = 0 to 199 scaled down using screen texel height (likely by 2)
    float color_index = floor(texture(screen_tex, uv).x * 255.0 / 16.0 + 0.5);
    float scanline = floor((1.0 - uv.t) * screen_params.y / screen_params.w + 0.5);
    vec2 color_uv = vec2(
      (color_index + 0.5) * color_params.z / color_params.x,
      1.0 - (scanline + 0.5) * color_params.w / color_params.y);
    frag_color = texture(color_tex, color_uv);
}
)glsl";
