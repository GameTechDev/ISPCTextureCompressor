/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>

struct rgba_surface 
{
    uint8_t* ptr;
    int32_t width;
    int32_t height;
    int32_t stride; // in bytes
};

struct bc7_enc_settings
{
    bool mode_selection[4];
    int refineIterations[8];

    bool skip_mode2;
    int fastSkipTreshold_mode1;
    int fastSkipTreshold_mode3;
    int fastSkipTreshold_mode7;

    int mode45_channel0;
    int refineIterations_channel;

    int channels;
};

struct bc6h_enc_settings
{
    bool slow_mode;
    bool fast_mode;
    int refineIterations_1p;
    int refineIterations_2p;
    int fastSkipTreshold;
};

struct etc_enc_settings
{
    int fastSkipTreshold;
};

struct astc_enc_settings
{
    int block_width;
    int block_height;
    int channels;

    int fastSkipTreshold;
    int refineIterations;
};

// profiles for RGB data (alpha channel will be ignored)
extern "C" void GetProfile_ultrafast(bc7_enc_settings* settings);
extern "C" void GetProfile_veryfast(bc7_enc_settings* settings);
extern "C" void GetProfile_fast(bc7_enc_settings* settings);
extern "C" void GetProfile_basic(bc7_enc_settings* settings);
extern "C" void GetProfile_slow(bc7_enc_settings* settings);

// profiles for RGBA inputs
extern "C" void GetProfile_alpha_ultrafast(bc7_enc_settings* settings);
extern "C" void GetProfile_alpha_veryfast(bc7_enc_settings* settings);
extern "C" void GetProfile_alpha_fast(bc7_enc_settings* settings);
extern "C" void GetProfile_alpha_basic(bc7_enc_settings* settings);
extern "C" void GetProfile_alpha_slow(bc7_enc_settings* settings);

// profiles for BC6H (RGB HDR)
extern "C" void GetProfile_bc6h_veryfast(bc6h_enc_settings* settings);
extern "C" void GetProfile_bc6h_fast(bc6h_enc_settings* settings);
extern "C" void GetProfile_bc6h_basic(bc6h_enc_settings* settings);
extern "C" void GetProfile_bc6h_slow(bc6h_enc_settings* settings);
extern "C" void GetProfile_bc6h_veryslow(bc6h_enc_settings* settings);

// profiles for ETC
extern "C" void GetProfile_etc_slow(etc_enc_settings* settings);

// profiles for ASTC
extern "C" void GetProfile_astc_fast(astc_enc_settings* settings, int block_width, int block_height);
extern "C" void GetProfile_astc_alpha_fast(astc_enc_settings* settings, int block_width, int block_height);
extern "C" void GetProfile_astc_alpha_slow(astc_enc_settings* settings, int block_width, int block_height);

// helper function to replicate border pixels for the desired block sizes (bpp = 32 or 64)
extern "C" void ReplicateBorders(rgba_surface* dst_slice, const rgba_surface* src_tex, int x, int y, int bpp);

/*
	Notes:
	  - input width and height need to be a multiple of block size
      - LDR input is 32 bit/pixel (sRGB), HDR is 64 bit/pixel (half float)
	  - dst buffer must be allocated with enough space for the compressed texture:
		4 bytes/block for BC1/ETC1, 8 bytes/block for BC3/BC6H/BC7/ASTC
		the blocks are stored in raster scan order (natural CPU texture layout)
	  - you can use GetProfile_* functions to select various speed/quality tradeoffs.
	  - the RGB profiles are slightly faster as they ignore the alpha channel
*/

extern "C" void CompressBlocksBC1(const rgba_surface* src, uint8_t* dst);
extern "C" void CompressBlocksBC3(const rgba_surface* src, uint8_t* dst);
extern "C" void CompressBlocksBC6H(const rgba_surface* src, uint8_t* dst, bc6h_enc_settings* settings);
extern "C" void CompressBlocksBC7(const rgba_surface* src, uint8_t* dst, bc7_enc_settings* settings);
extern "C" void CompressBlocksETC1(const rgba_surface* src, uint8_t* dst, etc_enc_settings* settings);
extern "C" void CompressBlocksASTC(const rgba_surface* src, uint8_t* dst, astc_enc_settings* settings);
