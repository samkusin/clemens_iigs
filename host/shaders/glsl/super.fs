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

  ((1.0 - V) * screen_params.y + 0.5 )/ screen_params.w

  foreach screen_tex pixel with (U, V) and COLOR:
      color_index = floor((sample(screen_tex, UV) * 255.0 / 16.0) + 0.5)
      scanline = floor((1.0 - V) * screen_params.y + 0.5);
      color_uv = (
        (color_index + 0.5) / 16.0f,
        1.0 - ((scanline + 0.5) * color_params.w) / color_tex.y
      )
      frag_color = sample(color_tex, color_uv) * COLOR
*/
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
