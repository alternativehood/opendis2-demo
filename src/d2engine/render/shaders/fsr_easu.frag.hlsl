// AMD FidelityFX FSR 1 EASU pass adapted to SDL's GPU 2D renderer contract.
// The algorithm and portability headers are vendored verbatim under third_party/fidelityfx-fsr.

cbuffer FsrEasuConstants : register(b0, space3) {
    uint4 con0;
    uint4 con1;
    uint4 con2;
    uint4 con3;
    uint2 output_size;
    uint2 padding;
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

#define A_GPU 1
#define A_HLSL 1
#include "fidelityfx-fsr/ffx-fsr/ffx_a.h"

#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return u_texture.GatherRed(u_sampler, p, int2(0, 0)); }
AF4 FsrEasuGF(AF2 p) { return u_texture.GatherGreen(u_sampler, p, int2(0, 0)); }
AF4 FsrEasuBF(AF2 p) { return u_texture.GatherBlue(u_sampler, p, int2(0, 0)); }
#include "fidelityfx-fsr/ffx-fsr/ffx_fsr1.h"

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    AF3 color;
    const AU2 pixel = AU2(input.v_uv * AF2(output_size));
    FsrEasuF(color, pixel, con0, con1, con2, con3);
    const float alpha = u_texture.Sample(u_sampler, input.v_uv).a;
    return float4(color, alpha) * input.v_color;
}
