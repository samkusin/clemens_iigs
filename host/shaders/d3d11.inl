const char* VS_D3D11_VERTEX =
  "cbuffer Globals {\n"
  "    float2 render_dims;\n"
  "    float2 display_ratio;\n"
  "    float2 virtual_dims;\n"
  "    float2 offsets;\n"
  "};\n"
  "struct Input {\n"
  "    float2 pos: POSITION;\n"
  "    float2 uv: TEXCOORD0;\n"
  "    float4 color: COLOR0;\n"
  "};\n"
  "struct Output {\n"
  "    float2 uv: TEXCOORD0;\n"
  "    float4 color: COLOR0;\n"
  "    float4 pos: SV_POSITION;\n"
  "};\n"
  "Output main(Input input) {\n"
  "    Output output;\n"
  "    float2 t_pos = (input.pos * display_ratio + offsets) / render_dims;\n"
  "    t_pos = (t_pos - 0.5) * float2(2.0, -2.0);\n"
  "    output.pos = float4(t_pos, 0.5, 1.0);\n"
  "    output.uv = input.uv;\n"
  "    output.color = input.color;\n"
  "    return output;\n"
  "}\n";

const char* FS_D3D11_TEXT =
  "Texture2D<float4> tex: register(t0);\n"
  "sampler smp: register(s0);\n"
  "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
  "    float4 texl = tex.Sample(smp, uv);\n"
  "    return texl.xxxx * color;\n"
  "}\n";

const char* FS_D3D11_HIRES =
  "Texture2D<float4> hgr_tex: register(t0);\n"
  "Texture2D<float4> hcolor_tex: register(t1);\n"
  "sampler smp: register(s0);\n"
  "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
  "    float4 texl_hgr = hgr_tex.Sample(smp, uv);\n"
  "    float4 texl_color = hcolor_tex.Sample(smp, float2(texl_hgr.x, 0.0));\n"
  "    return texl_color;\n"
  "};\n";

const char* FS_D3D11_SUPER =
  "Texture2D<float4> hgr_tex: register(t0);\n"
  "Texture2D<float4> hcolor_tex: register(t1);\n"
  "sampler smp: register(s0);\n"
  "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
  "    float4 texl_hgr = hgr_tex.Sample(smp, uv);\n"
  "    float cx = ((texl_hgr.x*255.0) + 0.5)/255.0;\n"
  "    float4 texl_color = hcolor_tex.Sample(smp, float2(cx, 0.0));\n"
  "    return texl_color;\n"
  "};\n";
