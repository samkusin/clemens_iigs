const char *VS_VERTEX_SOURCE =
    "cbuffer Globals {\n"
    "    float2 render_dims;\n"
    "    float2 display_ratio;\n"
    "    float2 virtual_dims;\n"
    "    float2 offsets;\n"
    "};\n"
    "struct Input {\n"
    "    float2 pos: POSITION;\n"
    "    float2 uv: TEXCOORD1;\n"
    "    float4 color: COLOR1;\n"
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

const char *FS_TEXT_SOURCE =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "    float4 texl = tex.Sample(smp, uv);\n"
    "    return texl.xxxx * color;\n"
    "}\n";

const char *FS_HIRES_SOURCE =
    "Texture2D<float4> hgr_tex: register(t0);\n"
    "Texture2D<float4> hcolor_tex: register(t1);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "    float4 texl_hgr = hgr_tex.Sample(smp, uv);\n"
    "    float4 texl_color = hcolor_tex.Sample(smp, float2(texl_hgr.x, 0.0));\n"
    "    return texl_color;\n"
    "};\n";

const char *FS_SUPER_SOURCE = R"hlsl(
cbuffer Globals {
  float4 screen_params;
  float4 color_params;
};

Texture2D<float4> screen_tex : register(t0);
Texture2D<float4> color_tex : register(t1);
sampler smp : register(s0);

float4 main(float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_Target0 {
  //  color_index = 0 to 15
  //  scanline = 0 to 199 scaled down using screen texel height (likely by 2)
  float color_index = floor(screen_tex.Sample(smp, uv).x * 255.0 / 16.0 + 0.5);
  float scanline = floor(uv.y * screen_params.y / screen_params.w + 0.5);
  float2 color_uv =
      float2((color_index + 0.5) * color_params.z / color_params.x,
             (scanline + 0.5) * color_params.w / color_params.y);
  float4 texl_color = color_tex.Sample(smp, color_uv);
  return texl_color * color;
}
)hlsl";
