//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//
//--------------------------------------------------------------------------------------

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
