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

#include "processing.h"
#include "StopWatch.h" // Timer.
#include <strsafe.h>
#include <limits>

CompressionFunc* gCompressionFunc = CompressImageBC6H_basic;
bool gMultithreaded = true;

double gCompTime = 0.0;
double gCompRate = 0.0;
int gTexWidth = 0;
int gTexHeight = 0;
double gError = 0.0;
double gError2 = 0.0;

ID3D11ShaderResourceView* gUncompressedSRV = NULL;
ID3D11ShaderResourceView* gCompressedSRV = NULL;
ID3D11ShaderResourceView* gErrorSRV = NULL;

ID3D11InputLayout* gVertexLayout = NULL;
ID3D11Buffer* gQuadVB = NULL;
ID3D11Buffer* gConstantBuffer = NULL;
ID3D11VertexShader* gVertexShader = NULL;
ID3D11PixelShader* gRenderTexturePS = NULL;
ID3D11SamplerState* gSamPoint = NULL;

ID3D11DepthStencilState* gDepthStencilState = NULL;
UINT gStencilReference = 0;

// Win32 thread API
const int kMaxWinThreads = 64;

enum EThreadState {
	eThreadState_WaitForData,
	eThreadState_DataLoaded,
	eThreadState_Running,
	eThreadState_Done
};

struct WinThreadData {
	EThreadState state;
	int threadIdx;
	//int width;
	//int height;
	//void (*cmpFunc)(const BYTE* inBuf, BYTE* outBuf, int width, int height);
	CompressionFunc* cmpFunc;
	rgba_surface input;
	BYTE *output;

	// Defaults..
	WinThreadData() :
		state(eThreadState_Done),
		threadIdx(-1),
		input(),
		output(NULL),
		cmpFunc(NULL)
	{ }

} gWinThreadData[kMaxWinThreads];

HANDLE gWinThreadWorkEvent[kMaxWinThreads];
HANDLE gWinThreadStartEvent = NULL;
HANDLE gWinThreadDoneEvent = NULL;
int gNumWinThreads = 0;
DWORD gNumProcessors = 1; // We have at least one processor.
DWORD dwThreadIdArray[kMaxWinThreads];
HANDLE hThreadArray[kMaxWinThreads];

void Initialize()
{
	// Make sure that the event array is set to null...
	memset(gWinThreadWorkEvent, 0, sizeof(gWinThreadWorkEvent));

	// Figure out how many cores there are on this machine
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	gNumProcessors = sysinfo.dwNumberOfProcessors;

	// Make sure all of our threads are empty.
	for(int i = 0; i < kMaxWinThreads; i++) {
		hThreadArray[i] = NULL;
	}
}

// Free previously allocated texture resources and create new texture resources.
HRESULT CreateTextures(LPTSTR file)
{
	// Destroy any previously created textures.
	DestroyTextures();

	// Load the uncompressed texture.
	HRESULT hr;
	V_RETURN(LoadTexture(file));

	// Compress the texture.
	V_RETURN(CompressTexture(gUncompressedSRV, &gCompressedSRV));

	// Compute the error in the compressed texture.
	V_RETURN(ComputeError(gUncompressedSRV, gCompressedSRV, &gErrorSRV));

	return S_OK;
}

// Recompresses the already loaded texture and recomputes the error.
HRESULT RecompressTexture()
{
	// Destroy any previously created textures.
	SAFE_RELEASE(gErrorSRV);
	SAFE_RELEASE(gCompressedSRV);

	// Compress the texture.
	HRESULT hr;
	V_RETURN(CompressTexture(gUncompressedSRV, &gCompressedSRV));

	// Compute the error in the compressed texture.
	V_RETURN(ComputeError(gUncompressedSRV, gCompressedSRV, &gErrorSRV));

	return S_OK;
}

// Destroy texture resources.
void DestroyTextures()
{
	SAFE_RELEASE(gErrorSRV);
	SAFE_RELEASE(gCompressedSRV);
	SAFE_RELEASE(gUncompressedSRV);
}

// This functions loads a texture and prepares it for compression. The compressor only works on texture
// dimensions that are divisible by 4.  Textures that are not divisible by 4 are resized and padded with the edge values.
HRESULT LoadTexture(LPTSTR file)
{
	// Load the uncrompressed texture.
	// The loadInfo structure disables mipmapping by setting MipLevels to 1.
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	loadInfo.Width = D3DX11_DEFAULT;
	loadInfo.Height = D3DX11_DEFAULT;
	loadInfo.Depth = D3DX11_DEFAULT;
	loadInfo.FirstMipLevel = D3DX11_DEFAULT;
	loadInfo.MipLevels = 1;
	loadInfo.Usage = (D3D11_USAGE) D3DX11_DEFAULT;
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.CpuAccessFlags = D3DX11_DEFAULT;
	loadInfo.MiscFlags = D3DX11_DEFAULT;
    loadInfo.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	loadInfo.Filter = D3DX11_FILTER_POINT | D3DX11_FILTER_SRGB;
	loadInfo.MipFilter = D3DX11_DEFAULT;
	loadInfo.pSrcInfo = NULL;
	HRESULT hr;
	V_RETURN(D3DX11CreateShaderResourceViewFromFile(DXUTGetD3D11Device(), file, &loadInfo, NULL, &gUncompressedSRV, NULL));

	// Pad the texture.
	V_RETURN(PadTexture(&gUncompressedSRV));

	// Query the texture description.
	ID3D11Texture2D* tex;
	gUncompressedSRV->GetResource((ID3D11Resource**)&tex);
	D3D11_TEXTURE2D_DESC texDesc;
	tex->GetDesc(&texDesc);
	SAFE_RELEASE(tex);

	gTexWidth = texDesc.Width;
	gTexHeight = texDesc.Height;

	return S_OK;
}

// Pad the texture to dimensions that are divisible by 4.
HRESULT PadTexture(ID3D11ShaderResourceView** textureSRV)
{
	// Query the texture description.
	ID3D11Texture2D* tex;
	(*textureSRV)->GetResource((ID3D11Resource**)&tex);
	D3D11_TEXTURE2D_DESC texDesc;
	tex->GetDesc(&texDesc);

	// Exit if the texture dimensions are divisible by 4.
	if((texDesc.Width % 4 == 0) && (texDesc.Height % 4 == 0))
	{
		SAFE_RELEASE(tex);
		return S_OK;
	}

	// Compute the size of the padded texture.
	UINT padWidth = texDesc.Width / 4 * 4 + 4;
	UINT padHeight = texDesc.Height / 4 * 4 + 4;

	// Create a buffer for the padded texels.
	BYTE* padTexels = new BYTE[padWidth * padHeight * 4];

	// Create a staging resource for the texture.
	HRESULT hr;
	ID3D11Device* device = DXUTGetD3D11Device();
	D3D11_TEXTURE2D_DESC stgTexDesc;
	memcpy(&stgTexDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	stgTexDesc.Usage = D3D11_USAGE_STAGING;
	stgTexDesc.BindFlags = 0;
	stgTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* stgTex;
	V_RETURN(device->CreateTexture2D(&stgTexDesc, NULL, &stgTex));

	// Copy the texture into the staging resource.
    ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
	deviceContext->CopyResource(stgTex, tex);

	// Map the staging resource.
	D3D11_MAPPED_SUBRESOURCE texData;
	V_RETURN(deviceContext->Map(stgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &texData));

	// Copy the beginning of each row.
	BYTE* texels = (BYTE*)texData.pData;
	for(UINT row = 0; row < stgTexDesc.Height; row++)
	{
		UINT rowStart = row * texData.RowPitch;
		UINT padRowStart = row * padWidth * 4;
		memcpy(padTexels + padRowStart, texels + rowStart, stgTexDesc.Width * 4); 

		// Pad the end of each row.
		if(padWidth > stgTexDesc.Width)
		{
			BYTE* padVal = texels + rowStart + (stgTexDesc.Width - 1) * 4;
			for(UINT padCol = stgTexDesc.Width; padCol < padWidth; padCol++)
			{
				UINT padColStart = padCol * 4;
				memcpy(padTexels + padRowStart + padColStart, padVal, 4);
			}
		}
	}

	// Pad the end of each column.
	if(padHeight > stgTexDesc.Height)
	{
		UINT lastRow = (stgTexDesc.Height - 1);
		UINT lastRowStart = lastRow * padWidth * 4;
		BYTE* padVal = padTexels + lastRowStart;
		for(UINT padRow = stgTexDesc.Height; padRow < padHeight; padRow++)
		{
			UINT padRowStart = padRow * padWidth * 4;
			memcpy(padTexels + padRowStart, padVal, padWidth * 4);
		}
	}

	// Unmap the staging resources.
	deviceContext->Unmap(stgTex, D3D11CalcSubresource(0, 0, 1));

	// Create a padded texture.
	D3D11_TEXTURE2D_DESC padTexDesc;
	memcpy(&padTexDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	padTexDesc.Width = padWidth;
	padTexDesc.Height = padHeight;
	D3D11_SUBRESOURCE_DATA padTexData;
	ZeroMemory(&padTexData, sizeof(D3D11_SUBRESOURCE_DATA));
	padTexData.pSysMem = padTexels;
	padTexData.SysMemPitch = padWidth * sizeof(BYTE) * 4;
	ID3D11Texture2D* padTex;
	V_RETURN(device->CreateTexture2D(&padTexDesc, &padTexData, &padTex));

	// Delete the padded texel buffer.
	delete [] padTexels;

	// Release the shader resource view for the texture.
	SAFE_RELEASE(*textureSRV);

	// Create a shader resource view for the padded texture.
	D3D11_SHADER_RESOURCE_VIEW_DESC padTexSRVDesc;
	padTexSRVDesc.Format = padTexDesc.Format;
	padTexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	padTexSRVDesc.Texture2D.MipLevels = padTexDesc.MipLevels;
	padTexSRVDesc.Texture2D.MostDetailedMip = padTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(padTex, &padTexSRVDesc, textureSRV));

	// Release resources.
	SAFE_RELEASE(padTex);
	SAFE_RELEASE(stgTex);
	SAFE_RELEASE(tex);

	return S_OK;
}

// Save a texture to a file.
HRESULT SaveTexture(ID3D11ShaderResourceView* textureSRV, LPTSTR file)
{
	// Get the texture resource.
	ID3D11Resource* texRes;
	textureSRV->GetResource(&texRes);
	if(texRes == NULL)
	{
		return E_POINTER;
	}

	// Save the texture to a file.
	HRESULT hr;
	V_RETURN(D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), texRes, D3DX11_IFF_DDS, file));

	// Release the texture resources.
	SAFE_RELEASE(texRes);

	return S_OK;
}

// Compress a texture.
HRESULT CompressTexture(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView** compressedSRV)
{
	// Query the texture description of the uncompressed texture.
	ID3D11Resource* uncompRes;
	gUncompressedSRV->GetResource(&uncompRes);
	D3D11_TEXTURE2D_DESC uncompTexDesc;
	((ID3D11Texture2D*)uncompRes)->GetDesc(&uncompTexDesc);

	// Create a 2D texture for the compressed texture.
	HRESULT hr;
	ID3D11Texture2D* compTex;
	D3D11_TEXTURE2D_DESC compTexDesc;
	memcpy(&compTexDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	
	compTexDesc.Format = GetFormatFromCompressionFunc(gCompressionFunc);
	
	ID3D11Device* device = DXUTGetD3D11Device();
	V_RETURN(device->CreateTexture2D(&compTexDesc, NULL, &compTex));

	// Create a shader resource view for the compressed texture.
	SAFE_RELEASE(*compressedSRV);
	D3D11_SHADER_RESOURCE_VIEW_DESC compSRVDesc;
	compSRVDesc.Format = compTexDesc.Format;
	compSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	compSRVDesc.Texture2D.MipLevels = compTexDesc.MipLevels;
	compSRVDesc.Texture2D.MostDetailedMip = compTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(compTex, &compSRVDesc, compressedSRV));

	// Create a staging resource for the compressed texture.
	compTexDesc.Usage = D3D11_USAGE_STAGING;
	compTexDesc.BindFlags = 0;
	compTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* compStgTex;
	V_RETURN(device->CreateTexture2D(&compTexDesc, NULL, &compStgTex));

	// Create a staging resource for the uncompressed texture.
	uncompTexDesc.Usage = D3D11_USAGE_STAGING;
	uncompTexDesc.BindFlags = 0;
	uncompTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* uncompStgTex;
	V_RETURN(device->CreateTexture2D(&uncompTexDesc, NULL, &uncompStgTex));

	// Copy the uncompressed texture into the staging resource.
    ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
	deviceContext->CopyResource(uncompStgTex, uncompRes);

	// Map the staging resources.
	D3D11_MAPPED_SUBRESOURCE uncompData;
	V_RETURN(deviceContext->Map(uncompStgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &uncompData));
	D3D11_MAPPED_SUBRESOURCE compData;
	V_RETURN(deviceContext->Map(compStgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &compData));

	// Time the compression.
	StopWatch stopWatch;
	stopWatch.Start();

	const int kNumCompressions = 1;
	for(int cmpNum = 0; cmpNum < kNumCompressions; cmpNum++) 
	{
		rgba_surface input;
		input.ptr = (BYTE*)uncompData.pData;
		input.stride = uncompData.RowPitch;
		input.width = uncompTexDesc.Width;
		input.height = uncompTexDesc.Height;

		BYTE* output = (BYTE*)compData.pData;
		
		// Compress the uncompressed texels.
		CompressImage(&input, output);

		// remap for DX (expanding inplace, bottom-up)
		int output_stride = (input.width/4)*GetBytesPerBlock(gCompressionFunc);
		for (int y=input.height/4-1; y>=0; y--)
		{
			memmove(output+y*compData.RowPitch, output+y*output_stride, output_stride);
		}
	}

	// Update the compression time.
	stopWatch.Stop();
	gCompTime = stopWatch.TimeInMilliseconds();

	// Compute the compression rate.
	INT numPixels = compTexDesc.Width * compTexDesc.Height * kNumCompressions;
	gCompRate = (double)numPixels / stopWatch.TimeInSeconds() / 1000000.0;
	stopWatch.Reset();

	// Unmap the staging resources.
	deviceContext->Unmap(compStgTex, D3D11CalcSubresource(0, 0, 1));
	deviceContext->Unmap(uncompStgTex, D3D11CalcSubresource(0, 0, 1));

	// Copy the staging resourse into the compressed texture.
	deviceContext->CopyResource(compTex, compStgTex);

	// Release resources.
	SAFE_RELEASE(uncompStgTex);
	SAFE_RELEASE(compStgTex);
	SAFE_RELEASE(compTex);
	SAFE_RELEASE(uncompRes);

	return S_OK;
}

static inline DXGI_FORMAT GetNonSRGBFormat(DXGI_FORMAT f) {
	switch(f) {
		case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM; 
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: assert(!"Unknown format!");
	}
	return DXGI_FORMAT_R8G8B8A8_UNORM;
}

void SetIdentity(D3DXMATRIX* mView)
{
	ZeroMemory(mView, sizeof(D3DXMATRIX));

	mView->m[0][0] = 1;
	mView->m[1][1] = 1;
	mView->m[2][2] = 1;
	mView->m[3][3] = 1;
}

inline float half2float(uint16_t value)
{
    float out;
    int abs = value & 0x7FFF;
    if (abs > 0x7C00)
        out = std::numeric_limits<float>::quiet_NaN();
    else if (abs == 0x7C00)
        out = std::numeric_limits<float>::infinity();
    else if (abs > 0x3FF)
        out = std::ldexp(static_cast<float>((value & 0x3FF) | 0x400), (abs >> 10) - 15 - 10);
    else
        out = std::ldexp(static_cast<float>(abs), -24);
    return (value & 0x8000) ? -out : out;
}

float sq(float x) { return x*x; }
float log2f(float x) { return logf(x)/logf(2); }

void ComputeErrorMetrics(rgba_surface* input, rgba_surface* raw)
{
    float metric = 0.0f;

    for (int y = 0; y < input->height; y++)
    for (int x = 0; x < input->width; x++)
    {
        uint16_t* rgb_a_fp16 = (uint16_t*)&input->ptr[input->stride*y + x * 8];
        uint16_t* rgb_b_fp16 = (uint16_t*)&raw->ptr[raw->stride*y + x * 8];

        float rgb_a[3], rgb_b[3];
        for (int p = 0; p < 3; p++)
        {
            rgb_a[p] = half2float(max(1, rgb_a_fp16[p]));
            rgb_b[p] = half2float(max(1, rgb_b_fp16[p]));
        }

        for (int p = 0; p < 3; p++)
        {
            float Ta = log2f(rgb_a[p]);
            float Tb = log2f(rgb_b[p]);
            metric += fabs(Ta - Tb);
        }
    }

    int samples = input->height*input->width * 3;
    gError = 100 * (metric / samples);
}

// Compute an "error" texture that represents the absolute difference in color between an
// uncompressed texture and a compressed texture.
HRESULT ComputeError(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView* compressedSRV, ID3D11ShaderResourceView** errorSRV)
{
	HRESULT hr;

	// Query the texture description of the uncompressed texture.
	ID3D11Resource* uncompRes;
	gUncompressedSRV->GetResource(&uncompRes);
	D3D11_TEXTURE2D_DESC uncompTexDesc;
	((ID3D11Texture2D*)uncompRes)->GetDesc(&uncompTexDesc);

	// Query the texture description of the uncompressed texture.
	ID3D11Resource* compRes;
	gCompressedSRV->GetResource(&compRes);
	D3D11_TEXTURE2D_DESC compTexDesc;
	((ID3D11Texture2D*)compRes)->GetDesc(&compTexDesc);

	// Create a 2D resource without gamma correction for the two textures.
    compTexDesc.Format = DXGI_FORMAT_BC6H_UF16;
    uncompTexDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	ID3D11Device* device = DXUTGetD3D11Device();

	ID3D11Texture2D* uncompTex;
	device->CreateTexture2D(&uncompTexDesc, NULL, &uncompTex);

	ID3D11Texture2D* compTex;
	device->CreateTexture2D(&compTexDesc, NULL, &compTex);

	// Create a shader resource view for the two textures.
	D3D11_SHADER_RESOURCE_VIEW_DESC compSRVDesc;
	compSRVDesc.Format = compTexDesc.Format;
	compSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	compSRVDesc.Texture2D.MipLevels = compTexDesc.MipLevels;
	compSRVDesc.Texture2D.MostDetailedMip = compTexDesc.MipLevels - 1;
	ID3D11ShaderResourceView *compSRV;
	V_RETURN(device->CreateShaderResourceView(compTex, &compSRVDesc, &compSRV));

	D3D11_SHADER_RESOURCE_VIEW_DESC uncompSRVDesc;
	uncompSRVDesc.Format = uncompTexDesc.Format;
	uncompSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	uncompSRVDesc.Texture2D.MipLevels = uncompTexDesc.MipLevels;
	uncompSRVDesc.Texture2D.MostDetailedMip = uncompTexDesc.MipLevels - 1;
	ID3D11ShaderResourceView *uncompSRV;
	V_RETURN(device->CreateShaderResourceView(uncompTex, &uncompSRVDesc, &uncompSRV));

	// Create a 2D texture for the error texture.
	ID3D11Texture2D* errorTex;
	D3D11_TEXTURE2D_DESC errorTexDesc;
	memcpy(&errorTexDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	errorTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	V_RETURN(device->CreateTexture2D(&errorTexDesc, NULL, &errorTex));

	// Create a render target view for the error texture.
	D3D11_RENDER_TARGET_VIEW_DESC errorRTVDesc;
	errorRTVDesc.Format = errorTexDesc.Format;
	errorRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	errorRTVDesc.Texture2D.MipSlice = 0;
	ID3D11RenderTargetView* errorRTV;
	V_RETURN(device->CreateRenderTargetView(errorTex, &errorRTVDesc, &errorRTV));

	// Create a shader resource view for the error texture.
	D3D11_SHADER_RESOURCE_VIEW_DESC errorSRVDesc;
	errorSRVDesc.Format = errorTexDesc.Format;
	errorSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	errorSRVDesc.Texture2D.MipLevels = errorTexDesc.MipLevels;
	errorSRVDesc.Texture2D.MostDetailedMip = errorTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(errorTex, &errorSRVDesc, errorSRV));

	// Create a query for the GPU operations...
	D3D11_QUERY_DESC GPUQueryDesc;
	GPUQueryDesc.Query = D3D11_QUERY_EVENT;
	GPUQueryDesc.MiscFlags = 0;

#ifdef _DEBUG
	D3D11_QUERY_DESC OcclusionQueryDesc;
	OcclusionQueryDesc.Query = D3D11_QUERY_OCCLUSION;
	OcclusionQueryDesc.MiscFlags = 0;

	D3D11_QUERY_DESC StatsQueryDesc;
	StatsQueryDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
	StatsQueryDesc.MiscFlags = 0;
#endif

	ID3D11Query *GPUQuery;
	V_RETURN(device->CreateQuery(&GPUQueryDesc, &GPUQuery));

	ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();

	deviceContext->CopyResource(compTex, compRes);
	deviceContext->CopyResource(uncompTex, uncompRes);

#ifdef _DEBUG
	ID3D11Query *OcclusionQuery, *StatsQuery;
	V_RETURN(device->CreateQuery(&OcclusionQueryDesc, &OcclusionQuery));
	V_RETURN(device->CreateQuery(&StatsQueryDesc, &StatsQuery));

	deviceContext->Begin(OcclusionQuery);
	deviceContext->Begin(StatsQuery);
#endif	

	// Set the viewport to a 1:1 mapping of pixels to texels.
	D3D11_VIEWPORT viewport;
	viewport.Width = (FLOAT)errorTexDesc.Width;
	viewport.Height = (FLOAT)errorTexDesc.Height;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &viewport);

	// Bind the render target view of the error texture.
	ID3D11RenderTargetView* RTV[1] = { errorRTV };
	deviceContext->OMSetRenderTargets(1, RTV, NULL);

	// Clear the render target.
	FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(errorRTV, color);

	// Set the input layout.
	deviceContext->IASetInputLayout(gVertexLayout);

	// Set vertex buffer
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &gQuadVB, &stride, &offset);

	// Set the primitive topology
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Update the Constant Buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
    deviceContext->Map( gConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    VS_CONSTANT_BUFFER* pConstData = ( VS_CONSTANT_BUFFER* )MappedResource.pData;
	ZeroMemory(pConstData, sizeof(VS_CONSTANT_BUFFER));
	SetIdentity(&pConstData->mView);
	deviceContext->Unmap( gConstantBuffer, 0 );

    // Set the shaders
	ID3D11Buffer* pBuffers[1] = { gConstantBuffer };
	deviceContext->VSSetConstantBuffers( 0, 1, pBuffers );
	deviceContext->VSSetShader(gVertexShader, NULL, 0);
	deviceContext->PSSetShader(gRenderTexturePS, NULL, 0);

	// Set the texture sampler.
	deviceContext->PSSetSamplers(0, 1, &gSamPoint);

	// Bind the textures.
    ID3D11ShaderResourceView* SRV[2] = { uncompSRV, compSRV };
	deviceContext->PSSetShaderResources(0, 2, SRV);

	// Store the depth/stencil state.
	StoreDepthStencilState();

	// Disable depth testing.
	V_RETURN(DisableDepthTest());

	// Render a quad.
	deviceContext->Draw(6, 0);

	// Restore the depth/stencil state.
	RestoreDepthStencilState();

	// Reset the render target.
	RTV[0] = DXUTGetD3D11RenderTargetView();
    deviceContext->OMSetRenderTargets(1, RTV, DXUTGetD3D11DepthStencilView());

	// Reset the viewport.
	viewport.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
	viewport.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
	deviceContext->RSSetViewports(1, &viewport);

	deviceContext->End(GPUQuery);
#ifdef _DEBUG
	deviceContext->End(OcclusionQuery);
	deviceContext->End(StatsQuery);
#endif

	BOOL finishedGPU = false;

	// If we do not have a d3d 11 context, we will still hit this line and try to
	// finish using the GPU. If this happens this enters an infinite loop.
	int infLoopPrevention = 0;
	while(!finishedGPU && ++infLoopPrevention < 10000) {
		HRESULT ret;
		V_RETURN(ret = deviceContext->GetData(GPUQuery, &finishedGPU, sizeof(BOOL), 0));
		if(ret != S_OK)
			Sleep(1);
	}

#ifdef _DEBUG
	UINT64 nPixelsWritten = 0;
	deviceContext->GetData(OcclusionQuery, (void *)&nPixelsWritten, sizeof(UINT64), 0);

	D3D11_QUERY_DATA_PIPELINE_STATISTICS stats;
	deviceContext->GetData(StatsQuery, (void *)&stats, sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS), 0);

	TCHAR nPixelsWrittenMsg[256];
	_stprintf(nPixelsWrittenMsg, _T("Pixels rendered during error computation: %d\n"), nPixelsWritten);
	OutputDebugString(nPixelsWrittenMsg);
#endif

	// Create a copy of the error texture that is accessible by the CPU
	ID3D11Texture2D* errorTexCopy;
	D3D11_TEXTURE2D_DESC errorTexCopyDesc;
	memcpy(&errorTexCopyDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	errorTexCopyDesc.Usage = D3D11_USAGE_STAGING;
	errorTexCopyDesc.BindFlags = 0;
	errorTexCopyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	V_RETURN(device->CreateTexture2D(&errorTexCopyDesc, NULL, &errorTexCopy));

	// Copy the error texture into the copy....
	deviceContext->CopyResource(errorTexCopy, errorTex);

	// Map the staging resource.
	D3D11_MAPPED_SUBRESOURCE errorData;
	V_RETURN(deviceContext->Map(errorTexCopy, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &errorData));

	// Calculate PSNR
    //ComputeRMSE((const BYTE *)(errorData.pData), errorTexCopyDesc.Width, errorTexCopyDesc.Height);

	// Unmap the staging resources.
    deviceContext->Unmap(errorTexCopy, D3D11CalcSubresource(0, 0, 1));

    {
        // Create a staging resource for the uncompressed texture.
        //uncompTexDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uncompTexDesc.Usage = D3D11_USAGE_STAGING;
        uncompTexDesc.BindFlags = 0;
        uncompTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        ID3D11Texture2D* uncompStgTex;
        V_RETURN(device->CreateTexture2D(&uncompTexDesc, NULL, &uncompStgTex));

        // Copy the uncompressed texture into the staging resource.
        ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
        deviceContext->CopyResource(uncompStgTex, uncompRes);

        /*
        // Create a copy of the error texture that is accessible by the CPU
        ID3D11Texture2D* rawTexCopy;
        D3D11_TEXTURE2D_DESC rawTexCopyDesc;
        memcpy(&rawTexCopyDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
        rawTexCopyDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        rawTexCopyDesc.Usage = D3D11_USAGE_STAGING;
        rawTexCopyDesc.BindFlags = 0;
        rawTexCopyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        V_RETURN(device->CreateTexture2D(&rawTexCopyDesc, NULL, &rawTexCopy));


        ID3D11Resource* mehRes;
        gErrorSRV->GetResource(&mehRes);

        // Copy the error texture into the copy....
        deviceContext->CopyResource(rawTexCopy, mehRes);
        */

        // Map the staging resources.
        D3D11_MAPPED_SUBRESOURCE uncompData;
        V_RETURN(deviceContext->Map(uncompStgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &uncompData));

        // Map the staging resource.
        D3D11_MAPPED_SUBRESOURCE rawData;
        V_RETURN(deviceContext->Map(errorTexCopy, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &rawData));
        //V_RETURN(deviceContext->Map(rawTexCopy, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &rawData));

        rgba_surface input;
        input.ptr = (BYTE*)uncompData.pData;
        input.stride = uncompData.RowPitch;
        input.width = uncompTexDesc.Width;
        input.height = uncompTexDesc.Height;

        rgba_surface raw;
        raw.ptr = (BYTE*)rawData.pData;
        raw.stride = rawData.RowPitch;
        raw.width = uncompTexDesc.Width;
        raw.height = uncompTexDesc.Height;

        ComputeErrorMetrics(&input, &raw);

        deviceContext->Unmap(uncompStgTex, D3D11CalcSubresource(0, 0, 1));
        deviceContext->Unmap(errorTexCopy, D3D11CalcSubresource(0, 0, 1));

        SAFE_RELEASE(uncompStgTex);
        //SAFE_RELEASE(rawTexCopy);
    }

	// Release resources.
	SAFE_RELEASE(errorRTV);
	SAFE_RELEASE(errorTex);
	SAFE_RELEASE(errorTexCopy);
	SAFE_RELEASE(uncompRes);
	SAFE_RELEASE(compRes);
	SAFE_RELEASE(GPUQuery);

#ifdef _DEBUG
	SAFE_RELEASE(OcclusionQuery);
	SAFE_RELEASE(StatsQuery);
#endif

	SAFE_RELEASE(compSRV);
	SAFE_RELEASE(uncompSRV);
	SAFE_RELEASE(compTex);
	SAFE_RELEASE(uncompTex);

	return S_OK;
}

// Store the depth-stencil state.
void StoreDepthStencilState()
{
	DXUTGetD3D11DeviceContext()->OMGetDepthStencilState(&gDepthStencilState, &gStencilReference);
}

// Restore the depth-stencil state.
void RestoreDepthStencilState()
{
	DXUTGetD3D11DeviceContext()->OMSetDepthStencilState(gDepthStencilState, gStencilReference);
}

// Disable depth testing.
HRESULT DisableDepthTest()
{
	D3D11_DEPTH_STENCIL_DESC depStenDesc;
	ZeroMemory(&depStenDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depStenDesc.DepthEnable = FALSE;
	depStenDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depStenDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depStenDesc.StencilEnable = FALSE;
	depStenDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depStenDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depStenDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depStenDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	ID3D11DepthStencilState* depStenState;
	HRESULT hr;
	V_RETURN(DXUTGetD3D11Device()->CreateDepthStencilState(&depStenDesc, &depStenState));

    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState(depStenState, 0);

	SAFE_RELEASE(depStenState);

	return S_OK;
}

void ComputeRMSE(const BYTE *errorData, const INT width, const INT height)
{
    //double sum_sq = 0.0;
    double sum_log_abs = 0.0;
    //double sum_sq_alpha = 0.0;
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            const INT pixel = ((const INT *)errorData)[j * width + i];

            double dr = double(pixel & 0xFF);
            double dg = double((pixel >> 8) & 0xFF);
            double db = double((pixel >> 16) & 0xFF);
            double da = double((pixel >> 24) & 0xFF);

            //for (int i = 0; i < width; i++) {

            //sum_log_abs += ;
            //sum_sq_alpha += da*da;
        }
    }

    //sum_sq_rgb /= (double(width) * double(height));
    //sum_sq_alpha /= (double(width) * double(height));

    //gError = 10 * log10((255.0 * 255.0 * 3) / (sum_sq_rgb));
    gError = 0;
    gError2 = 0;
    //gError2 = 10 * log10((255.0 * 255.0 * 4) / (sum_sq_rgb + sum_sq_alpha));
}

#define CHECK_WIN_THREAD_FUNC(x) \
	do { \
		if(NULL == (x)) { \
			wchar_t wstr[256]; \
			swprintf_s(wstr, L"Error detected from call %s at line %d of main.cpp", _T(#x), __LINE__); \
			ReportWinThreadError(wstr); \
		} \
	} \
	while(0)

void ReportWinThreadError(const wchar_t *str) {

	// Retrieve the system error message for the last-error code.
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError(); 

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	// Display the error message.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
		(lstrlen((LPCTSTR) lpMsgBuf) + lstrlen((LPCTSTR)str) + 40) * sizeof(TCHAR)); 
	StringCchPrintf((LPTSTR)lpDisplayBuf, 
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"), 
		str, dw, lpMsgBuf); 
	MessageBox(NULL, (LPCTSTR) lpDisplayBuf, TEXT("Error"), MB_OK); 

	// Free error-handling buffer allocations.

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}

void InitWin32Threads() {

	// Already initialized?
	if(gNumWinThreads > 0) {
		return;
	}
	
	SetLastError(0);

	gNumWinThreads = gNumProcessors;

	if(gNumWinThreads >= MAXIMUM_WAIT_OBJECTS)
		gNumWinThreads = MAXIMUM_WAIT_OBJECTS;

	assert(gNumWinThreads <= kMaxWinThreads);

	// Create the synchronization events.
	for(int i = 0; i < gNumWinThreads; i++) {
		CHECK_WIN_THREAD_FUNC(gWinThreadWorkEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL));
	}

	CHECK_WIN_THREAD_FUNC(gWinThreadStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL));
	CHECK_WIN_THREAD_FUNC(gWinThreadDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL));

	// Create threads
	for(int threadIdx = 0; threadIdx < gNumWinThreads; threadIdx++) {
		gWinThreadData[threadIdx].state = eThreadState_WaitForData;
		CHECK_WIN_THREAD_FUNC(hThreadArray[threadIdx] = CreateThread(NULL, 0, CompressImageMT_Thread, &gWinThreadData[threadIdx], 0, &dwThreadIdArray[threadIdx]));
	}
}

void DestroyThreads()
{
	if(gMultithreaded) 
	{
		// Release all windows threads that may be active...
		for(int i=0; i < gNumWinThreads; i++) {
			gWinThreadData[i].state = eThreadState_Done;
		}

		// Send the event for the threads to start.
		CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadDoneEvent));
		CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadStartEvent));

		// Wait for all the threads to finish....
		DWORD dwWaitRet = WaitForMultipleObjects(gNumWinThreads, hThreadArray, TRUE, INFINITE);
		if(WAIT_FAILED == dwWaitRet)
			ReportWinThreadError(L"DestroyThreads() -- WaitForMultipleObjects");

		// !HACK! This doesn't actually do anything. There is either a bug in the 
		// Intel compiler or the windows run-time that causes the threads to not
		// be cleaned up properly if the following two lines of code are not present.
		// Since we're passing INFINITE to WaitForMultipleObjects, that function will
		// never time out and per-microsoft spec, should never give this return value...
		// Even with these lines, the bug does not consistently disappear unless you
		// clean and rebuild. Heigenbug?
		//
		// If we compile with MSVC, then the following two lines are not necessary.
		else if(WAIT_TIMEOUT == dwWaitRet)
			OutputDebugString(L"DestroyThreads() -- WaitForMultipleObjects -- TIMEOUT");

		// Reset the start event
		CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadStartEvent));
		CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadDoneEvent));

		// Close all thread handles.
		for(int i=0; i < gNumWinThreads; i++) {
			CHECK_WIN_THREAD_FUNC(CloseHandle(hThreadArray[i]));
		}

		for(int i =0; i < kMaxWinThreads; i++ ){
			hThreadArray[i] = NULL;
		}

		// Close all event handles...
		CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadDoneEvent)); 
		gWinThreadDoneEvent = NULL;
			
		CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadStartEvent)); 
		gWinThreadStartEvent = NULL;

		for(int i = 0; i < gNumWinThreads; i++) {
			CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadWorkEvent[i]));
		}

		for(int i = 0; i < kMaxWinThreads; i++) {
			gWinThreadWorkEvent[i] = NULL;
		}

		gNumWinThreads = 0;
	}
}

VOID CompressImage(const rgba_surface* input, BYTE* output) 
{
	// If we aren't multi-cored, then just run everything serially.
	if(gNumProcessors <= 1 || !gMultithreaded) 
	{
		CompressImageST(input, output);
	}
	else
	{
		CompressImageMT(input, output);
	}
}

void CompressImageST(const rgba_surface* input, BYTE* output) 
{
	if(gCompressionFunc == NULL) {
		OutputDebugString(L"DXTC::CompressImageDXTNoThread -- Compression Scheme not implemented!\n");
		return;
	}

	// Do the compression.
	(*gCompressionFunc)(input, output);
}

VOID CompressImageMT(const rgba_surface* input, BYTE* output) 
{
	const int numThreads = gNumWinThreads;
	const int bytesPerBlock = GetBytesPerBlock(gCompressionFunc);

	// We want to split the data evenly among all threads.
	const int linesPerThread = (input->height + numThreads - 1) / numThreads;

	if(gCompressionFunc == NULL) {
		OutputDebugString(L"DXTC::CompressImageDXTNoThread -- Compression Scheme not implemented!\n");
		return;
	}

	// Load the threads.
	for(int threadIdx = 0; threadIdx < numThreads; threadIdx++) 
	{
		int y_start = (linesPerThread*threadIdx)/4*4;
		int y_end = (linesPerThread*(threadIdx+1))/4*4;
		if (y_end > input->height) y_end = input->height;
		
		WinThreadData *data = &gWinThreadData[threadIdx];
		data->input = *input;
		data->input.ptr = input->ptr + y_start * input->stride;
		data->input.height = y_end-y_start;
		data->output = output + (y_start/4) * (input->width/4) * bytesPerBlock;
		data->cmpFunc = gCompressionFunc;
		data->state = eThreadState_DataLoaded;
		data->threadIdx = threadIdx;
	}

	// Send the event for the threads to start.
	CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadDoneEvent));
	CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadStartEvent));

	// Wait for all the threads to finish
	if(WAIT_FAILED == WaitForMultipleObjects(numThreads, gWinThreadWorkEvent, TRUE, INFINITE))
			ReportWinThreadError(TEXT("CompressImageDXTWIN -- WaitForMultipleObjects"));

	// Reset the start event
	CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadStartEvent));
	CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadDoneEvent));
}

DWORD WINAPI CompressImageMT_Thread( LPVOID lpParam ) {
	WinThreadData *data = (WinThreadData *)lpParam;

	while(data->state != eThreadState_Done) {

		if(WAIT_FAILED == WaitForSingleObject(gWinThreadStartEvent, INFINITE))
			ReportWinThreadError(TEXT("CompressImageDXTWinThread -- WaitForSingleObject"));

		if(data->state == eThreadState_Done)
			break;

		data->state = eThreadState_Running;
		(*(data->cmpFunc))(&data->input, data->output);

		data->state = eThreadState_WaitForData;

		HANDLE workEvent = gWinThreadWorkEvent[data->threadIdx];
		if(WAIT_FAILED == SignalObjectAndWait(workEvent, gWinThreadDoneEvent, INFINITE, FALSE))
			ReportWinThreadError(TEXT("CompressImageDXTWinThread -- SignalObjectAndWait"));
	}

	return 0;
}

int GetBytesPerBlock(CompressionFunc* fn)
{
	DXGI_FORMAT format = GetFormatFromCompressionFunc(fn);

	switch(format) 
	{
		default:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return 8;
				
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_BC6H_UF16:
            return 16;
	}
}

DXGI_FORMAT GetFormatFromCompressionFunc(CompressionFunc* fn)
{
    return DXGI_FORMAT_BC6H_UF16;
}

#define DECLARE_CompressImageBC6H_profile(profile)								\
void CompressImageBC6H_ ## profile(const rgba_surface* input, BYTE* output)		\
{																				\
	bc6h_enc_settings settings;													\
	GetProfile_bc6h_ ## profile(&settings);										\
	CompressBlocksBC6H(input, output, &settings);								\
}

DECLARE_CompressImageBC6H_profile(veryfast);
DECLARE_CompressImageBC6H_profile(fast);
DECLARE_CompressImageBC6H_profile(basic);
DECLARE_CompressImageBC6H_profile(slow);
DECLARE_CompressImageBC6H_profile(veryslow);