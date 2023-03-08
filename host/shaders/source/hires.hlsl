Texture2D<float4> hgr_tex: register(t0);
Texture2D<float> hcolor_tex: register(t1);
sampler smp: register(s0);

float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {
  float4 texl_hgr = hgr_tex.Sample(smp, uv);
  float4 texl_color = hcolor_tex.Sample(smp, texl_hgr.x / 7.0);
  return texl_color * color;
}
