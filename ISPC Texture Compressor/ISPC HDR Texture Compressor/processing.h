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

#pragma once

#include "DXUT.h"
#include <tchar.h>
#include "ispc_texcomp.h"

typedef void (CompressionFunc)(const rgba_surface* input, BYTE* output);

extern CompressionFunc* gCompressionFunc;
extern bool gMultithreaded;

extern double gCompTime;
extern double gCompRate;
extern int gTexWidth;
extern int gTexHeight;
extern double gError;
extern double gError2;

extern ID3D11ShaderResourceView* gUncompressedSRV; // Shader resource view for the uncompressed texture resource.
extern ID3D11ShaderResourceView* gCompressedSRV; // Shader resource view for the compressed texture resource.
extern ID3D11ShaderResourceView* gErrorSRV; // Shader resource view for the error texture.

// Textured vertex.
struct Vertex
{
    D3DXVECTOR3 position;
	D3DXVECTOR2 texCoord;
};

struct VS_CONSTANT_BUFFER
{
	D3DXMATRIX mView;
    float exposure;
    float padding[3];
};

extern ID3D11InputLayout* gVertexLayout;
extern ID3D11Buffer* gQuadVB;
extern ID3D11Buffer* gConstantBuffer;
extern ID3D11VertexShader* gVertexShader;
extern ID3D11PixelShader* gRenderTexturePS;
extern ID3D11SamplerState* gSamPoint;

void Initialize();
HRESULT CreateTextures(LPTSTR file);
void DestroyTextures();
HRESULT LoadTexture(LPTSTR file);
HRESULT PadTexture(ID3D11ShaderResourceView** textureSRV);
HRESULT SaveTexture(ID3D11ShaderResourceView* textureSRV, LPTSTR file);
HRESULT CompressTexture(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView** compressedSRV);
HRESULT ComputeError(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView* compressedSRV, ID3D11ShaderResourceView** errorSRV);
HRESULT RecompressTexture();

void ComputeRMSE(const BYTE *errorData, const INT width, const INT height);

void InitWin32Threads();
void DestroyThreads();

void StoreDepthStencilState();
void RestoreDepthStencilState();
HRESULT DisableDepthTest();

VOID CompressImage(const rgba_surface* input, BYTE* output);
VOID CompressImageST(const rgba_surface* input, BYTE* output);
VOID CompressImageMT(const rgba_surface* input, BYTE* output);
DWORD WINAPI CompressImageMT_Thread( LPVOID lpParam );

int GetBytesPerBlock(CompressionFunc* fn);
DXGI_FORMAT GetFormatFromCompressionFunc(CompressionFunc* fn);

void CompressImageBC6H_veryfast(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_fast(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_basic(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_slow(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_veryslow(const rgba_surface* input, BYTE* output);
