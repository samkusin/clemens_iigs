cbuffer Globals {
    float2 render_dims;     //  Render Target Resolution
    float2 display_ratio;   //  Ratio convert virtual to target
    float2 virtual_dims;    //  Emulator 'pixel' resolution
    float2 offsets;         //  x,y offset from top, left
};

struct Input {
    float2 pos: POSITION;
    float2 uv: TEXCOORD0;
    float4 color: COLOR0;
};

struct Output {
    float2 uv: TEXCOORD0;
    float4 color: COLOR0;
    float4 pos: SV_POSITION;
};

Output main(Input input) {
    Output output;
    float2 t_pos = (input.pos * display_ratio + offsets) / render_dims;
    t_pos = (t_pos - 0.5) * float2(2.0, -2.0);
    output.pos = float4(t_pos, 0.5, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
