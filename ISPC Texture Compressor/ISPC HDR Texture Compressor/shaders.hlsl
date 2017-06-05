////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
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
float4 RenderTexturePS(VS_OUTPUT Input) : SV_TARGET
{
    float4 uncompressed = gTexture.Sample(gSampler, Input.vTexcoord);
    float4 compressed = gCompressedTexture.Sample(gSampler, Input.vTexcoord);

    //float4 error = abs(compressed - uncompressed);
    //float4 logA = log(compressed);
    //float4 logB = log(uncompressed);
    //float4 error = abs(logA - logB);

    return compressed;
}
