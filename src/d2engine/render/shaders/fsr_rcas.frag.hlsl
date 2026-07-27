// AMD FidelityFX FSR 1 RCAS pass adapted to SDL's GPU 2D renderer contract.
// The algorithm and portability headers are vendored verbatim under third_party/fidelityfx-fsr.

cbuffer FsrRcasConstants : register(b0, space3) {
    uint4 con0;
    uint2 output_size;
    uint2 padding;
};

Texture2D u_texture : register(t0, space2);

#define A_GPU 1
#define A_HLSL 1
#define FSR_RCAS_F 1
#define FSR_RCAS_PASSTHROUGH_ALPHA 1
#include "fidelityfx-fsr/ffx-fsr/ffx_a.h"

AF4 FsrRcasLoadF(ASU2 p) {
    const int2 max_pixel = int2(output_size) - int2(1, 1);
    return u_texture.Load(int3(clamp(int2(p), int2(0, 0), max_pixel), 0));
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "fidelityfx-fsr/ffx-fsr/ffx_fsr1.h"

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    AF1 red;
    AF1 green;
    AF1 blue;
    AF1 alpha;
    FsrRcasF(red, green, blue, alpha, AU2(input.v_uv * AF2(output_size)), con0);
    return float4(red, green, blue, alpha) * input.v_color;
}
