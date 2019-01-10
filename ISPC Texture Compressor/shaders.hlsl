////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

Texture2D gTexture : register(t0);
Texture2D gCompressedTexture : register(t1);

SamplerState gSampler : register( s0 );

cbuffer cb0
{
    row_major float4x4 mView;
    float exposure_mul;
};

struct VS_INPUT
{
    float4 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

// Pass-through vertex shader.
VS_OUTPUT PassThroughVS(VS_INPUT Input)
{
    VS_OUTPUT Output;

    Output.vPosition = mul(Input.vPosition, mView);
    Output.vTexcoord = Input.vTexcoord;

    return Output;
}

// Display pixel shader.
float4 RenderFramePS(VS_OUTPUT Input) : SV_TARGET
{
    float4 texel = gTexture.Sample(gSampler, Input.vTexcoord);

    return texel*exposure_mul;
}

float4 RenderAlphaPS(VS_OUTPUT Input) : SV_TARGET
{
    float4 texel = gTexture.Sample(gSampler, Input.vTexcoord);

    float alpha = texel.a*texel.a; // compensate for gamma

    return float4(alpha.xxx, 1.0f);
}

// Render to texture pixel shader.
float4 RenderErrorTexturePS(VS_OUTPUT Input) : SV_TARGET
{
    float4 uncompressed = gTexture.Sample(gSampler, Input.vTexcoord);
    float4 compressed = gCompressedTexture.Sample(gSampler, Input.vTexcoord);
    float4 error = abs(compressed - uncompressed);

    return error;
}

// Render to texture pixel shader.
float4 RenderCompressedTexturePS(VS_OUTPUT Input) : SV_TARGET
{
    float4 compressed = gCompressedTexture.Sample(gSampler, Input.vTexcoord);
    return compressed;
}
