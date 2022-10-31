const char* VS_VERTEX_SOURCE =
  "#version 330\n"
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

  const char* FS_TEXT_SOURCE =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec4 color;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = texture(tex, uv).xxxx * color;\n"
    "}\n";

  const char* FS_HIRES_SOURCE =
    "#version 330\n"
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

  const char* FS_SUPER_SOURCE =
    "#version 330\n"
    "uniform sampler2D hgr_tex;\n"
    "uniform sampler2D hcolor_tex;\n"
    "in vec4 color;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec4 texl_hgr = texture(hgr_tex, uv);\n"
    "  float cx = ((texl_hgr.x * 255.0) + 0.5) / 255.0;\n"
    "  frag_color = texture(hcolor_tex, vec2(cx, 0.0));\n"
    "}\n";
