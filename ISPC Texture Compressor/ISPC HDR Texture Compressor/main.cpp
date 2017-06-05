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

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <tchar.h>

#include "processing.h"

// Global variables
CDXUTDialogResourceManager gDialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg gD3DSettingsDlg; // Device settings dialog
CDXUTDialog gHUD; // manages the 3D   
CDXUTDialog gSampleUI; // dialog for sample specific controls
bool gShowHelp = false; // If true, it renders the UI control text
CDXUTTextHelper* gTxtHelper = NULL;
int gFrameNum = 0;
int gFrameDelay = 100;

int gSurfaceWidth;
int gSurfaceHeight;

enum EImageView
{
	eImageView_Uncompressed,
	eImageView_Compressed,
	eImageView_Error,
	eImageView_All,

	kNumImageView
};

EImageView gImageView = eImageView_Compressed;
int gViewZoomStep = 0;
int gViewTranslationX = 0;
int gViewTranslationY = 0;
float gLog2Exposure = 0;

ID3D11Buffer* gVertexBuffer = NULL;
ID3D11Buffer* gIndexBuffer = NULL;
ID3D11PixelShader* gRenderFramePS = NULL;
ID3D11PixelShader* gRenderAlphaPS = NULL;

enum
{	
	IDC_TOGGLEFULLSCREEN,
	IDC_TOGGLEREF,
	IDC_CHANGEDEVICE,

	IDC_TEXT,	
	IDC_PROFILE,
	IDC_MT,
	IDC_RECOMPRESS,
	IDC_IMAGEVIEW,
    IDC_ALPHA,
    IDC_EXPOSURE,
    IDC_LOAD_TEXTURE,
	IDC_SAVE_TEXTURE
};

// Forward declarations 
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, 
					   bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext );

void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

void FillProfiles(BOOL DX11Available);

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Initialize();

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
	DXUTSetCallbackMouse( OnMouse, true );
	DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();

	if (gMultithreaded)
	{
		InitWin32Threads();
	}

    DXUTInit( true, true, NULL );
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"ISPC HDR Texture Compressor" );

	// Try to create a device with DX11 feature set
    DXUTCreateDevice (D3D_FEATURE_LEVEL_11_0, true, 1280, 1024 );

	BOOL DX11Available = false;

	// If we don't have an adequate driver, then we revert to DX10 feature set...
	DXUTDeviceSettings settings = DXUTGetDeviceSettings();
	if(settings.d3d11.DriverType == D3D_DRIVER_TYPE_UNKNOWN || settings.d3d11.DriverType == D3D_DRIVER_TYPE_NULL) {
		DXUTCreateDevice(D3D_FEATURE_LEVEL_10_1, true, 1280, 1024);

		// !HACK! Force enumeration here in order to relocate hardware with new feature level
		DXUTGetD3D11Enumeration(true);
		DXUTCreateDevice(D3D_FEATURE_LEVEL_10_1, true, 1280, 1024);

		const TCHAR *noDx11msg = _T("Your hardware does not seem to support DX11. BC7 Compression is disabled.");
		MessageBox(NULL, noDx11msg, _T("Error"), MB_OK);
	}
	else
	{
		DX11Available = true;
	}
	
	FillProfiles(DX11Available);

    DXUTMainLoop();

	// Destroy all of the threads...
	DestroyThreads();

    return DXUTGetExitCode();
}

// Initialize the app 
void InitApp()
{
	// Initialize dialogs
	gD3DSettingsDlg.Init(&gDialogResourceManager);
	gHUD.Init(&gDialogResourceManager);
	gSampleUI.Init(&gDialogResourceManager);

	gHUD.SetCallback(OnGUIEvent);
	int x = 0;
	int y = 10;
	gHUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", x, y, 170, 23);
	gHUD.AddButton(IDC_TOGGLEREF, L"Toggle REF (F3)", x, y += 26, 170, 23, VK_F3);
	gHUD.AddButton(IDC_CHANGEDEVICE, L"Change device (F2)", x, y += 26, 170, 23, VK_F2);
	gHUD.SetSize( 170, 170 );
	
	gSampleUI.SetCallback(OnGUIEvent);
	x = 0;
	y = 0;
    gSampleUI.AddStatic(IDC_TEXT, L"", x, y, 1, 1); y += 5*22;
	gSampleUI.AddComboBox(IDC_PROFILE, x, y, 226, 22); y += 26;
	gSampleUI.AddCheckBox(IDC_MT, L"Multithreaded", x, y, 125, 22, gMultithreaded);
	gSampleUI.AddButton(IDC_RECOMPRESS, L"Recompress", x + 131, y, 125, 22); y += 26;
	gSampleUI.AddComboBox(IDC_IMAGEVIEW, x, y, 145, 22);
    gSampleUI.AddCheckBox(IDC_ALPHA, L"Show Alpha", x + 151, y, 105, 22); y += 26;
    gSampleUI.AddSlider(IDC_EXPOSURE, x, y, 250, 22); y += 26;
	gSampleUI.AddButton(IDC_LOAD_TEXTURE, L"Load Texture", x, y, 125, 22);
	gSampleUI.AddButton(IDC_SAVE_TEXTURE, L"Save Texture", x + 131, y, 125, 22); y += 26;

	gSampleUI.SetSize( 276, y+150 );

	{
		CDXUTComboBox *comboBox = gSampleUI.GetComboBox(IDC_IMAGEVIEW);
		comboBox->AddItem(L"Uncompressed", (void *)(eImageView_Uncompressed));
		comboBox->AddItem(L"Compressed", (void *)(eImageView_Compressed));
		//comboBox->AddItem(L"Error", (void *)(eImageView_Error));
		//comboBox->AddItem(L"All", (void *)(eImageView_All));
		comboBox->SetSelectedByData((void *)(gImageView));
	}

	gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));
}

void FillProfiles(BOOL DX11Available)
{
	CDXUTComboBox *comboBox = gSampleUI.GetComboBox(IDC_PROFILE);
	if (DX11Available)
	{		
        comboBox->AddItem(L"BC6H veryfast", (void *)(CompressImageBC6H_veryfast));
        comboBox->AddItem(L"BC6H fast", (void *)(CompressImageBC6H_fast));
        comboBox->AddItem(L"BC6H basic", (void *)(CompressImageBC6H_basic));
        comboBox->AddItem(L"BC6H slow", (void *)(CompressImageBC6H_slow));
        comboBox->AddItem(L"BC6H veryslow", (void *)(CompressImageBC6H_veryslow));

		comboBox->SetDropHeight((12-1)*17);
	}

	comboBox->SetSelectedByData((void *)(gCompressionFunc));
}

// Called right before creating a D3D11 device, allowing the app to modify the device settings as needed
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

// Render the help and statistics text
void RenderText()
{
    UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
            DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    gTxtHelper->Begin();
    gTxtHelper->SetInsertionPos( 2, 0 );
    gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    gTxtHelper->DrawTextLine( DXUTGetFrameStats( false ) );
    gTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( gShowHelp )
    {
        gTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 20 * 6 );
        gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        gTxtHelper->DrawTextLine( L"Controls:" );

        gTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 20 * 5 );
        gTxtHelper->DrawTextLine(	L"Switch Compressed/Uncompressed views: TAB\n"
									L"Hide help: F1\n"
                                    L"Quit: ESC\n" );
    }
    else
    {
        gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        gTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    gTxtHelper->End();
}

// Handle messages to the application
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = gDialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = gHUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = gSampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    return 0;
}

// Handle key presses
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                gShowHelp = !gShowHelp; break;
			case VK_TAB:
				{
					CDXUTComboBox *comboBox = gSampleUI.GetComboBox(IDC_IMAGEVIEW);
					if (eImageView_Uncompressed == (intptr_t)comboBox->GetSelectedData())
					{
						comboBox->SetSelectedByData((void*)eImageView_Compressed);
					} 
					else if (eImageView_Compressed == (intptr_t)comboBox->GetSelectedData())
					{
						comboBox->SetSelectedByData((void*)eImageView_Uncompressed);
					}
					gSampleUI.SendEvent(IDC_IMAGEVIEW, true, comboBox);
					break;
				}
        }
    }
}

// image drag/zoom logic
bool gDragging = false;
bool gDragging_out = false;
int gLast_xPos = 0;
int gLast_yPos = 0;

void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, 
					   bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext )
{
	if (!gDragging_out && gDragging && bLeftButtonDown)
	{
		int delta_xPos = xPos-gLast_xPos;
		int delta_yPos = yPos-gLast_yPos;

		gViewTranslationX += delta_xPos;
		gViewTranslationY += delta_yPos;
	}
	
	if (nMouseWheelDelta)
	{
		float halfWidth = gSurfaceWidth / 2.0f;
		float halfHeight = gSurfaceHeight / 2.0f;

		int oldzoom = gViewZoomStep;
		gViewZoomStep += nMouseWheelDelta;

		float old_scale = powf(2.0, oldzoom/300.0f);
		float new_scale = powf(2.0, gViewZoomStep/300.0f);

		float vx = gViewTranslationX-(xPos-halfWidth);
		float vy = gViewTranslationY-(yPos-halfHeight);

		vx *= new_scale/old_scale;
		vy *= new_scale/old_scale;		

		gViewTranslationX = int(vx+xPos-halfWidth);
		gViewTranslationY = int(vy+yPos-halfHeight);
	}

	POINT base, pt = {xPos, yPos};
	gSampleUI.GetLocation(base);
	pt.x -= base.x;
	pt.y -= base.y;
	CDXUTControl* ctrl = gSampleUI.GetControlAtPoint(pt);

	if (!gDragging)
	{
		gDragging_out = !!ctrl && bLeftButtonDown;
	}

	gDragging = bLeftButtonDown;
	gLast_xPos = xPos;
	gLast_yPos = yPos;
}

void SetView(D3DXMATRIX* mView)
{
	ZeroMemory(mView, sizeof(D3DXMATRIX));

	float scale = powf(2.0, gViewZoomStep/300.0f);

	float pixelFracX = 2.0f/gSurfaceWidth;
	float pixelFracY = 2.0f/gSurfaceHeight;

	D3D11_TEXTURE2D_DESC uncompTexDesc;
	{
		ID3D11Resource* uncompRes;
		gUncompressedSRV->GetResource(&uncompRes);
		((ID3D11Texture2D*)uncompRes)->GetDesc(&uncompTexDesc);
		SAFE_RELEASE(uncompRes);
	}

	float imgscaleX = 1.0f*uncompTexDesc.Width/gSurfaceWidth;
	float imgscaleY = 1.0f*uncompTexDesc.Height/gSurfaceHeight;
	
	mView->m[0][0] = scale*imgscaleX;
    mView->m[1][1] = scale*imgscaleY;
    mView->m[2][2] = scale;
    mView->m[3][0] = pixelFracX*gViewTranslationX;
    mView->m[3][1] = -pixelFracY*gViewTranslationY;
    mView->m[3][3] = 1;
}

// Handles the GUI events
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
		{
            DXUTToggleFullScreen();
			break;
		}
        case IDC_TOGGLEREF:
		{
            DXUTToggleREF();
			break;
		}
        case IDC_CHANGEDEVICE:
		{
            gD3DSettingsDlg.SetActive( !gD3DSettingsDlg.IsActive() );
			break;
		}
        case IDC_TEXT:
        case IDC_EXPOSURE:
        {
            gLog2Exposure = (gSampleUI.GetSlider(IDC_EXPOSURE)->GetValue() / 100.0f - 0.5f) * 33.33333;
			WCHAR wstr[MAX_PATH];
			swprintf_s(wstr, MAX_PATH, 
				L"Texture Size: %d x %d\n" 
                L"logRGB L1: %.2f%%\n"
                //L"logRGB RMSE: %.4f\n"
                //L"Relative error: %.2f%%\n"
                //L"mPSNR: %.2f%%\n"
                L"Exposure: %.2f\n"
				L"Compression Time: %0.2f ms\n"
				L"Compression Rate: %0.2f Mp/s\n", 
				gTexWidth, gTexHeight,
                gError, gLog2Exposure,
				gCompTime, gCompRate);
			gSampleUI.GetStatic(IDC_TEXT)->SetText(wstr);
			break;
		}
		case IDC_MT:
		{
			// Shut down all previous threading abilities.
			DestroyThreads();
			
			gMultithreaded = gSampleUI.GetCheckBox(IDC_MT)->GetChecked();
					
			if (gMultithreaded)
			{
				InitWin32Threads();
			}

			// Recompress the texture.
			RecompressTexture();
			gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));

			break;
		}
		case IDC_PROFILE:
		{ 
			gCompressionFunc = (CompressionFunc*)gSampleUI.GetComboBox(IDC_PROFILE)->GetSelectedData();

			// Recompress the texture.
			RecompressTexture();
			gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));

			break;
		}
		case IDC_LOAD_TEXTURE:
		{
			// Store the current working directory.
			TCHAR workingDirectory[MAX_PATH];
			GetCurrentDirectory(MAX_PATH, workingDirectory);

			// Open a file dialog.
			OPENFILENAME openFileName;
			WCHAR file[MAX_PATH];
			file[0] = 0;
			ZeroMemory(&openFileName, sizeof(OPENFILENAME));
			openFileName.lStructSize = sizeof(OPENFILENAME);
			openFileName.lpstrFile = file;
			openFileName.nMaxFile = MAX_PATH;
			openFileName.lpstrFilter = L"DDS\0*.dds\0\0";
			openFileName.nFilterIndex = 1;
			openFileName.lpstrInitialDir = NULL;
			openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			if(GetOpenFileName(&openFileName))
			{
				//CreateTextures(openFileName.lpstrFile);
                SAFE_RELEASE(gUncompressedSRV);
                LoadTexture(openFileName.lpstrFile);
			}

			// Restore the working directory. GetOpenFileName changes the current working directory which causes problems with relative paths to assets.
			SetCurrentDirectory(workingDirectory);

            RecompressTexture();
			gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));
			
			break;
		}
		case IDC_RECOMPRESS:
		{
			// Recompress the texture.
			RecompressTexture();
			gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));

			break;
		}
		case IDC_SAVE_TEXTURE:
		{
			// Store the current working directory.
			TCHAR workingDirectory[MAX_PATH];
			GetCurrentDirectory(MAX_PATH, workingDirectory);

			// Open a file dialog.
			OPENFILENAME openFileName;
			WCHAR file[MAX_PATH];
			file[0] = 0;
			ZeroMemory(&openFileName, sizeof(OPENFILENAME));
			openFileName.lStructSize = sizeof(OPENFILENAME);
			openFileName.lpstrFile = file;
			openFileName.nMaxFile = MAX_PATH;
			openFileName.lpstrFilter = L"DDS\0*.dds\0\0";
			openFileName.lpstrDefExt = L"dds";
			openFileName.nFilterIndex = 1;
			openFileName.lpstrInitialDir = NULL;
			openFileName.Flags = OFN_PATHMUSTEXIST;
            
			if(GetSaveFileName(&openFileName))
			{
				SaveTexture(gCompressedSRV,openFileName.lpstrFile);
			}

			// Restore the working directory. GetOpenFileName changes the current working directory which causes problems with relative paths to assets.
			SetCurrentDirectory(workingDirectory);

			break;
		}
		case IDC_IMAGEVIEW:
		{
			gImageView = (EImageView)(INT_PTR)gSampleUI.GetComboBox(IDC_IMAGEVIEW)->GetSelectedData();

			break;
		}
	}
}

// Reject any D3D11 devices that aren't acceptable by returning false
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

// Find and compile the specified shader
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

// Create any D3D11 resources that aren't dependent on the back buffer
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN(gDialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
    V_RETURN(gD3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    gTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &gDialogResourceManager, 15);

    // Create a vertex shader.
    ID3DBlob* vertexShaderBuffer = NULL;
    V_RETURN(CompileShaderFromFile(L"shaders.hlsl", "PassThroughVS", "vs_4_0", &vertexShaderBuffer));
    V_RETURN(pd3dDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &gVertexShader));

	// Create a pixel shader that renders the composite frame.
    ID3DBlob* pixelShaderBuffer = NULL;
    V_RETURN(CompileShaderFromFile(L"shaders.hlsl", "RenderFramePS", "ps_4_0", &pixelShaderBuffer));
    V_RETURN(pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &gRenderFramePS));

	// Create a pixel shader that renders the error texture.
    V_RETURN(CompileShaderFromFile(L"shaders.hlsl", "RenderTexturePS", "ps_4_0", &pixelShaderBuffer));
    V_RETURN(pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &gRenderTexturePS));

	// Create a pixel shader that shows alpha
    V_RETURN(CompileShaderFromFile(L"shaders.hlsl", "RenderAlphaPS", "ps_4_0", &pixelShaderBuffer));
    V_RETURN(pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &gRenderAlphaPS));

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &gVertexLayout));

    SAFE_RELEASE(vertexShaderBuffer);
    SAFE_RELEASE(pixelShaderBuffer);

	// Create a vertex buffer for three textured quads.
	D3DXVECTOR2 quadSize(0.32f, 0.32f);
	D3DXVECTOR2 quadOrigin(-0.66f, -0.0f);
    Vertex tripleQuadVertices[18];
	ZeroMemory(tripleQuadVertices, sizeof(tripleQuadVertices));
	for(int i = 0; i < 18; i += 6)
	{
		tripleQuadVertices[i].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i].texCoord = D3DXVECTOR2(0.0f, 0.0f);

		tripleQuadVertices[i + 1].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i + 1].texCoord = D3DXVECTOR2(1.0f, 0.0f);

		tripleQuadVertices[i + 2].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 2].texCoord = D3DXVECTOR2(1.0f, 1.0f);

		tripleQuadVertices[i + 3].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 3].texCoord = D3DXVECTOR2(1.0f, 1.0f);

		tripleQuadVertices[i + 4].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 4].texCoord = D3DXVECTOR2(0.0f, 1.0f);

		tripleQuadVertices[i + 5].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i + 5].texCoord = D3DXVECTOR2(0.0f, 0.0f);

		quadOrigin.x += 0.66f;
	}

    D3D11_BUFFER_DESC bufDesc;
	ZeroMemory(&bufDesc, sizeof(bufDesc));
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.ByteWidth = sizeof(tripleQuadVertices);
    bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
    data.pSysMem = tripleQuadVertices;
    V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, &data, &gVertexBuffer));

	// Create a vertex buffer for a single textured quad.
	quadSize = D3DXVECTOR2(1.0f, 1.0f);
	quadOrigin = D3DXVECTOR2(0.0f, 0.0f);
	Vertex singleQuadVertices[6];
	singleQuadVertices[0].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[0].texCoord = D3DXVECTOR2(0.0f, 0.0f);
	singleQuadVertices[1].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[1].texCoord = D3DXVECTOR2(1.0f, 0.0f);
	singleQuadVertices[2].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[2].texCoord = D3DXVECTOR2(1.0f, 1.0f);
	singleQuadVertices[3].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[3].texCoord = D3DXVECTOR2(1.0f, 1.0f);
	singleQuadVertices[4].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[4].texCoord = D3DXVECTOR2(0.0f, 1.0f);
	singleQuadVertices[5].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[5].texCoord = D3DXVECTOR2(0.0f, 0.0f);

	ZeroMemory(&bufDesc, sizeof(bufDesc));
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.ByteWidth = sizeof(singleQuadVertices);
    bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = 0;
	ZeroMemory(&data, sizeof(data));
    data.pSysMem = singleQuadVertices;
    V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, &data, &gQuadVB));

    // Create a constant buffer
    ZeroMemory(&bufDesc, sizeof(bufDesc));
    bufDesc.ByteWidth = sizeof( VS_CONSTANT_BUFFER );
    bufDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, NULL, &gConstantBuffer));

    // Create a sampler state
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN(pd3dDevice->CreateSamplerState(&SamDesc, &gSamPoint));

	// Load and initialize the textures.
    WCHAR path[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"Images\\Desk_21_hdr.dds"));
    V_RETURN(CreateTextures(path));

	// Update the UI's texture width and height.
	gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));

    return S_OK;
}

// Create any D3D11 resources that depend on the back buffer
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;
    V_RETURN( gDialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( gD3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	gSurfaceWidth = pBackBufferSurfaceDesc->Width;
	gSurfaceHeight = pBackBufferSurfaceDesc->Height;

    gHUD.SetLocation( gSurfaceWidth - gHUD.GetWidth(), 0 );
	gSampleUI.SetLocation( gSurfaceWidth-gSampleUI.GetWidth(), gSurfaceHeight-gSampleUI.GetHeight() );

    return S_OK;
}

// Render the scene using the D3D11 device
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
	// Recompress the texture gFrameDelay frames after the app has started.  This produces more accurate timing of the
	// compression algorithm.
	if(gFrameNum == gFrameDelay)
	{
		RecompressTexture();
		gSampleUI.SendEvent(IDC_TEXT, true, gSampleUI.GetStatic(IDC_TEXT));
		gFrameNum++;
	}
	else if(gFrameNum < gFrameDelay)
	{
		gFrameNum++;
	}

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.02f, 0.02f, 0.02f, 1.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set the input layout.
    pd3dImmediateContext->IASetInputLayout( gVertexLayout );

    // Set the vertex buffer.
    UINT stride = sizeof( Vertex );
    UINT offset = 0;
	if (gImageView == eImageView_All)
	{
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &gVertexBuffer, &stride, &offset );
	}
	else
	{
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &gQuadVB, &stride, &offset );
	}
	
    // Set the primitive topology
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	// Update the Constant Buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( gConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    VS_CONSTANT_BUFFER* pConstData = ( VS_CONSTANT_BUFFER* )MappedResource.pData;
	ZeroMemory(pConstData, sizeof(VS_CONSTANT_BUFFER));
	SetView(&pConstData->mView);
    pConstData->exposure = powf(2.0, gLog2Exposure);
	pd3dImmediateContext->Unmap( gConstantBuffer, 0 );

    // Set the shaders
	ID3D11Buffer* pBuffers[1] = { gConstantBuffer };
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, pBuffers );
    pd3dImmediateContext->VSSetShader( gVertexShader, NULL, 0 );

	if (gSampleUI.GetCheckBox(IDC_ALPHA)->GetChecked())
	{
		pd3dImmediateContext->PSSetShader( gRenderAlphaPS, NULL, 0 );
	}
	else
	{
		pd3dImmediateContext->PSSetShader( gRenderFramePS, NULL, 0 );
	}
    
    pd3dImmediateContext->PSSetConstantBuffers(0, 1, pBuffers );

	// Set the texture sampler.
    pd3dImmediateContext->PSSetSamplers( 0, 1, &gSamPoint );

	// Render the textures.

	if (gImageView == eImageView_Uncompressed || gImageView == eImageView_All )
	{
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &gUncompressedSRV );
	}
	else if (gImageView == eImageView_Compressed)
	{
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &gCompressedSRV );
	}
	else if (gImageView == eImageView_Error)
	{
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &gErrorSRV );
    }

    pd3dImmediateContext->Draw( 6, 0 );

	if (gImageView == eImageView_All)
	{
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &gCompressedSRV );
		pd3dImmediateContext->Draw( 6, 6 );

		pd3dImmediateContext->PSSetShaderResources( 0, 1, &gErrorSRV );
		pd3dImmediateContext->Draw( 6, 12 );
	}	

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    HRESULT hr;
    V(gHUD.OnRender( fElapsedTime ));
    V(gSampleUI.OnRender( fElapsedTime ));
    RenderText();
    DXUT_EndPerfEvent();
}

// Release D3D11 resources created in OnD3D11ResizedSwapChain 
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}

// Release D3D11 resources created in OnD3D11CreateDevice 
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( gTxtHelper );

    SAFE_RELEASE( gVertexLayout );
    SAFE_RELEASE( gVertexBuffer );
    SAFE_RELEASE( gQuadVB );
    SAFE_RELEASE( gIndexBuffer );
    SAFE_RELEASE( gConstantBuffer );
    SAFE_RELEASE( gVertexShader );
    SAFE_RELEASE( gRenderFramePS );
    SAFE_RELEASE( gRenderTexturePS );
    SAFE_RELEASE( gRenderAlphaPS );
    SAFE_RELEASE( gSamPoint );

	DestroyTextures();
}
