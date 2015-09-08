/*
  Copyright (c) 2015, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.


   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
*/

typedef int8 int8_t;
typedef int32 int32_t;
typedef int64 int64_t;

typedef unsigned int8 uint8_t;
typedef unsigned int32 uint32_t;
typedef unsigned int64 uint64_t;

///////////////////////////
//   generic helpers

void swap(float& a, float& b)
{
    int t = a;
    a = b; b = t;
}

void swap(int& a, int& b)
{
    int t = a;
    a = b; b = t;
}

void swap(uint32_t& a, uint32_t& b)
{
    uint32_t t = a;
    a = b; b = t;
}

void swap(uint8_t& a, uint8_t& b)
{
    uint8_t t = a;
    a = b; b = t;
}

inline float sq(float v)
{
    return v*v;
}

inline float clamp(float v, int a, int b)
{
    return clamp(v, (float)a, (float)b);
}

inline float dot3(float a[3], float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// the following helpers isolate performance warnings

inline uint32_t gather_uint(const uniform uint32_t* const uniform ptr, int idx)
{
    return ptr[idx]; // (perf warning expected)
}

inline float gather_float(uniform float* uniform ptr, int idx)
{
    return ptr[idx]; // (perf warning expected)
}

inline float gather_float(varying float* uniform ptr, int idx)
{
    return ptr[idx]; // (perf warning expected)
}

inline void scatter_uint(uniform uint32_t* ptr, int idx, uint32_t value)
{
    ptr[idx] = value; // (perf warning expected)
}

inline void scatter_float(uniform float* uniform ptr, int idx, float value)
{
    ptr[idx] = value; // (perf warning expected)
}

inline void scatter_float(varying float* uniform ptr, int idx, float value)
{
    ptr[idx] = value; // (perf warning expected)
}

///////////////////////////////////////////////////////////
//				 ASTC shared functions

struct rgba_surface
{
    uint8_t* ptr;
    int width, height, stride;
};

inline void set_pixel(float pixels[], uniform int p, uniform int x, uniform int y, float value);

inline void load_block_interleaved(float pixels[], uniform rgba_surface src[], int xx, int yy, uniform int width, uniform int height)
{
    uniform int pitch = width * height;
    for (uniform int y = 0; y < height; y++)
    for (uniform int x = 0; x < width; x++)
    {
        uint32_t rgba = gather_uint((uint32_t*)src->ptr, ((yy * height + y)*src->stride + (xx * width + x) * 4)/4);

        set_pixel(pixels, 0, x, y, (int)((rgba >> 0) & 255));
        set_pixel(pixels, 1, x, y, (int)((rgba >> 8) & 255));
        set_pixel(pixels, 2, x, y, (int)((rgba >> 16) & 255));
    }
}

struct astc_enc_settings
{
    int block_width;
    int block_height;

    int fastSkipTreshold;
    int refineIterations;
};

export uniform int get_programCount()
{
    return programCount;
} 

inline float get_pixel(float pixels[], uniform int p, uniform int x, uniform int y)
{
    uniform static const int ystride = 8;
    uniform static const int pstride = 64;

    return pixels[pstride * p + ystride * y + x];
}

inline void set_pixel(float pixels[], uniform int p, uniform int x, uniform int y, float value)
{
    uniform static const int ystride = 8;
    uniform static const int pstride = 64;

    pixels[pstride * p + ystride * y + x] = value;
}

struct pixel_set
{
    varying float* uniform pixels;

    uniform int width;
    uniform int height;
};

inline void clear_alpha(float pixels[], uniform int width, uniform int height)
{
    for (uniform int y = 0; y < height; y++)
    for (uniform int x = 0; x < width; x++)
    {
        set_pixel(pixels, 3, x, y, 0);
    }
}

void rotate_plane(pixel_set block[], int p)
{
    uniform int pitch = block->height * block->width;

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float r = get_pixel(block->pixels, 0, x, y);
        float g = get_pixel(block->pixels, 1, x, y);
        float b = get_pixel(block->pixels, 2, x, y);
        float a = get_pixel(block->pixels, 3, x, y);
        
        if (p == 0) swap(a, r);
        if (p == 1) swap(a, g);
        if (p == 2) swap(a, b);

        set_pixel(block->pixels, 0, x, y, r);
        set_pixel(block->pixels, 1, x, y, g);
        set_pixel(block->pixels, 2, x, y, b);
        set_pixel(block->pixels, 3, x, y, a);        
    }
}

inline void compute_moments(float stats[15], pixel_set block[])
{
    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float rgba[3];
        for (uniform int p = 0; p < 3; p++) rgba[p] = get_pixel(block->pixels, p, x, y);

        stats[10] += rgba[0];
        stats[11] += rgba[1];
        stats[12] += rgba[2];

        stats[0] += rgba[0] * rgba[0];
        stats[1] += rgba[0] * rgba[1];
        stats[2] += rgba[0] * rgba[2];

        stats[4] += rgba[1] * rgba[1];
        stats[5] += rgba[1] * rgba[2];

        stats[7] += rgba[2] * rgba[2];
    }

    stats[14] += block->height * block->width;
}

inline void covar_from_stats(float covar[10], float stats[15])
{
    covar[0] = stats[0] - stats[10 + 0] * stats[10 + 0] / stats[14];
    covar[1] = stats[1] - stats[10 + 0] * stats[10 + 1] / stats[14];
    covar[2] = stats[2] - stats[10 + 0] * stats[10 + 2] / stats[14];

    covar[4] = stats[4] - stats[10 + 1] * stats[10 + 1] / stats[14];
    covar[5] = stats[5] - stats[10 + 1] * stats[10 + 2] / stats[14];

    covar[7] = stats[7] - stats[10 + 2] * stats[10 + 2] / stats[14];
}

inline void compute_covar_dc(float covar[], float dc[], pixel_set block[], bool zero_based)
{
    float stats[15] = { 0 };
    compute_moments(stats, block);

    if (zero_based)
    for (uniform int p = 0; p < 3; p++) stats[10 + p] = 0;

    covar_from_stats(covar, stats);
    for (uniform int p = 0; p < 3; p++) dc[p] = stats[10 + p] / stats[14];
}

inline void ssymv3(float a[4], float covar[10], float b[4])
{
    a[0] = covar[0] * b[0] + covar[1] * b[1] + covar[2] * b[2];
    a[1] = covar[1] * b[0] + covar[4] * b[1] + covar[5] * b[2];
    a[2] = covar[2] * b[0] + covar[5] * b[1] + covar[7] * b[2];
}

inline void compute_axis(float axis[4], float covar[10], uniform int powerIterations)
{
    float vec[4] = { 1, 1, 1, 1 };

    for (uniform int i = 0; i<powerIterations; i++)
    {
        ssymv3(axis, covar, vec);
        for (uniform int p = 0; p < 3; p++) vec[p] = axis[p];

        if (i % 2 == 1) // renormalize every other iteration
        {
            float norm_sq = 0;
            for (uniform int p = 0; p < 3; p++)
                norm_sq += axis[p] * axis[p];

            float rnorm = rsqrt(norm_sq);
            for (uniform int p = 0; p < 3; p++) vec[p] *= rnorm;
        }
    }

    for (uniform int p = 0; p < 3; p++) axis[p] = vec[p];
}

void compute_pca_endpoints(float ep[6], pixel_set block[], bool zero_based)
{
    float sum[3];
    float cov[10];
    compute_covar_dc(cov, sum, block, zero_based);

    uniform int powerIterations = 10;

    float dir[3];
    compute_axis(dir, cov, powerIterations);
    
    float ext[2] = { 1000, -1000 };

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float proj = 0;
        for (uniform int p = 0; p < 3; p++) proj += (get_pixel(block->pixels, p, x, y) - sum[p]) * dir[p];

        ext[0] = min(ext[0], proj);
        ext[1] = max(ext[1], proj);
    }

    if (ext[1] - 1.0f < ext[0])
    {
        ext[1] += 0.5f;
        ext[0] -= 0.5f;
    }
    
    for (uniform int i = 0; i < 2; i++)
    for (uniform int p = 0; p < 3; p++)
    {
        ep[p * 2 + i] = sum[p] + dir[p] * ext[i];
    }
}

uniform static const int range_table[][3] =
{
    //2^ 3^ 5^
    { 1, 0, 0 }, // 0..1
    { 0, 1, 0 }, // 0..2
    { 2, 0, 0 }, // 0..3

    { 0, 0, 1 }, // 0..4
    { 1, 1, 0 }, // 0..5
    { 3, 0, 0 }, // 0..7

    { 1, 0, 1 }, // 0..9
    { 2, 1, 0 }, // 0..11
    { 4, 0, 0 }, // 0..15

    { 2, 0, 1 }, // 0..19
    { 3, 1, 0 }, // 0..23
    { 5, 0, 0 }, // 0..31

    { 3, 0, 1 }, // 0..39
    { 4, 1, 0 }, // 0..47
    { 6, 0, 0 }, // 0..63

    { 4, 0, 1 }, // 0..79
    { 5, 1, 0 }, // 0..95
    { 7, 0, 0 }, // 0..127

    { 5, 0, 1 }, // 0..159
    { 6, 1, 0 }, // 0..191
    { 8, 0, 0 }, // 0..255
};

uniform int get_levels(uniform int range)
{
    return (1 + 2 * range_table[range][1] + 4 * range_table[range][2]) << range_table[range][0];
}

struct range_values
{
    int levels_m;
    int levels_m_rcp;
    int levels_e;
    int levels;
};

void fill_range_values(range_values values[], int _range[])
{
    int range = *_range;
    int range_div3 = (range * 21846) >> 16;
    int range_mod3 = range - range_div3 * 3;

    int levels_m = max(2, 5 - range_mod3 * 2);
    int levels_e = max(0, range_mod3 + range_div3 - 1);
    if (range == 0) levels_m = 2;

    int levels_m_rcp = 0x10000 / 2 + 1;
    if (levels_m == 3) levels_m_rcp = 0x10000 / 3 + 1;
    if (levels_m == 5) levels_m_rcp = 0x10000 / 5 + 1;

    values->levels_e = levels_e;
    values->levels_m = levels_m;
    values->levels_m_rcp = levels_m_rcp;

    values->levels = levels_m << levels_e;
}

range_values get_range_values(int range)
{
    range_values values;
    fill_range_values(&values, &range);
    return values;
}

int get_levels(int range)
{
    int range_div3 = (range * 21846) >> 16;
    int range_mod3 = range - range_div3 * 3;

    int levels_m = max(2, 5 - range_mod3 * 2);
    int levels_e = range_mod3 + range_div3 - 1;

    return (levels_m << (levels_e + 1)) >> 1;
}

uniform float get_sq_rcp_levels(uniform int range)
{
    uniform static const float table[] =
    {
        1.000000, 0.250000, 0.111111,
        0.062500, 0.040000, 0.020408,
        0.012346, 0.008264, 0.004444,
        0.002770, 0.001890, 0.001041,
        0.000657, 0.000453, 0.000252,
        0.000160, 0.000111, 0.000062,
        0.000040, 0.000027, 0.000015,
    };

    return table[range];
}

///////////////////////////////////////////////////////////
//				 ASTC candidate ranking

struct astc_rank_state
{
    float pixels[256];

    float pca_error[2][4];
    float sq_norm[2][4];
    float scale_error[7][7]; // 2x2 to 8x8

    float best_scores[64];
    uint32_t best_modes[64];

    // settings
    uniform int block_width;
    uniform int block_height;
    uniform int pitch;

    uniform int fastSkipTreshold;
    uniform int refineIterations;
};

struct astc_mode
{
    int width;
    int height;
    bool dual_plane;
    int weight_range;
    int color_component_selector;
    int partitions;
    int partition_id;
    int color_endpoint_pairs;
    int color_endpoint_modes[2];
    int endpoint_range;
};

void dct_4(float values[], uniform int stride)
{
    uniform static const float scale[] = { 0.5, 0.707106769 };
    uniform static const float c[5] = { 1, 0.923879533, 0.707106769, 0.382683432, 0 };
        
    float data[4];
    for (uniform int i = 0; i < 2; i++)
    {
        float a = values[stride * i];
        float b = values[stride * (3 - i)];
        data[0 + i] = a + b;
        data[2 + i] = a - b;
    }

    for (uniform int i = 0; i < 4; i++)
    {
        float acc = 0;
        varying float* uniform input = &data[(i % 2) * 2];
        for (uniform int j = 0; j < 2; j++)
        {
            uniform int e = (2 * j + 1)*i;
            e = e % (4 * 4);
            uniform float w = 1;
            if (e>8) { e = 16 - e; }
            if (e>4) { w = -1;  e = 8 - e; }
            w *= c[e];
            acc += w * input[j];
        }

        values[stride * i] = acc * scale[i > 0];
    }
}

void dct_6(float values[], uniform int stride)
{
    uniform static const float scale[] = { 0.408248290, 0.577350269 };
    uniform static const float c[7] =
        { 1, 0.965925813, 0.866025388, 0.707106769, 0.500000000, 0.258819044, 0 };
    
    float data[6];
    for (uniform int i = 0; i < 3; i++)
    {
        float a = values[stride * i];
        float b = values[stride * (5 - i)];
        data[0 + i] = a + b;
        data[3 + i] = a - b;
    }

    for (uniform int i = 0; i < 6; i++)
    {
        float acc = 0;
        varying float* uniform input = &data[(i % 2) * 3];
        for (uniform int j = 0; j < 3; j++)
        {
            uniform int e = (2 * j + 1)*i;
            e = e % (4 * 6);
            uniform float w = 1;
            if (e>12) { e = 24 - e; }
            if (e>6) { w = -1;  e = 12 - e; }
            w *= c[e];
            acc += w * input[j];
        }

        values[stride * i] = acc * scale[i > 0];
    }
}

void dct_n(float values[], uniform int stride, uniform int n)
{
    uniform static const float pi = 3.14159265358979323846;

    assert(n <= 16);
    uniform float c[16 + 1];
    for (uniform int i = 0; i <= n; i++)
        c[i] = cos((i / (4.0 * n) * 2 * pi));

    uniform float scale[] = { 1 / sqrt(1.0*n), 1 / sqrt(n / 2.0), };

    float data[16];
    for (uniform int i = 0; i < n; i++)
        data[i] = values[stride * i];

    for (uniform int i = 0; i < n; i++)
    {
        float acc = 0;
        for (uniform int j = 0; j < n; j++)
        {
            uniform int e = (2 * j + 1)*i;
            e = e % (4 * n);
            float w = 1;
            if (e > 2 * n) { e = 4 * n - e; }
            if (e > n) { w = -1;  e = 2 * n - e; }
            assert(e <= n);
            w *= c[e];
            acc += w * data[j];
        }

        values[stride * i] = acc * scale[i > 0];
    }
}

void dct(float values[], uniform int stride, uniform int n)
{
    if (false) {}
    else if (n == 8) dct_n(values, stride, 8);
    else if (n == 6) dct_6(values, stride);
    else if (n == 5) dct_n(values, stride, 5);
    else if (n == 4) dct_4(values, stride);
    else
    {
        assert(false);
    }
}

void compute_dct_inplace(pixel_set block[])
{
    uniform static const int stride = 8;
    uniform static const int pitch = 64;

    for (uniform int p = 0; p < 3; p++)
    {
        for (uniform int y = 0; y < block->height; y++)
            dct(&block->pixels[pitch * p + y * stride], 1, block->width);

        for (uniform int x = 0; x < block->width; x++)
            dct(&block->pixels[pitch * p + x], stride, block->height);
    }
}

void compute_metrics(astc_rank_state state[])
{
    float temp_pixels[256];
    pixel_set _pset; varying pixel_set* uniform pset = &_pset;
    pset->pixels = temp_pixels;
    pset->width = state->block_width;
    pset->height = state->block_height;

    for (uniform int p = 0; p < 4; p++)
    for (uniform int y = 0; y < state->block_height; y++)
    for (uniform int x = 0; x < state->block_width; x++)
    {
        float value = get_pixel(state->pixels, p, x, y);
        set_pixel(pset->pixels, p, x, y, value);
    }

    for (uniform int i = 0; i < 2; i++)
    for (uniform int c = 0; c < 4; c++)
    {
        if (c > 0) rotate_plane(pset, c - 1);
        
        bool zero_based = (i==1);
        float endpoints[6];
        compute_pca_endpoints(endpoints, pset, zero_based);

        float base[3], dir[3];
        for (int p = 0; p < 3; p++) dir[p] = endpoints[p * 2 + 1] - endpoints[p * 2];
        for (int p = 0; p < 3; p++) base[p] = endpoints[p * 2];
        float sq_norm = dot3(dir, dir);

        float pca_error = 0;
        {
            for (uniform int y = 0; y < state->block_height; y++)
            for (uniform int x = 0; x < state->block_width; x++)
            {
                float pixel[3];
                for (uniform int p = 0; p < 3; p++) pixel[p] = get_pixel(pset->pixels, p, x, y) - base[p];
                float proj = dot3(pixel, dir) / sq_norm;
                for (uniform int p = 0; p < 3; p++) pca_error += sq(get_pixel(pset->pixels, p, x, y) - (proj * dir[p] + base[p]));
            }
        }

        state->pca_error[i][c] = pca_error;
        state->sq_norm[i][c] = sq_norm;

        if (c > 0)
        {
            float ext[2] = { 1000, -1000 };
            for (uniform int y = 0; y < state->block_height; y++)
            for (uniform int x = 0; x < state->block_width; x++)
            {
                float value = get_pixel(pset->pixels, 3, x, y);
                ext[0] = min(ext[0], value);
                ext[1] = max(ext[1], value);
            }

            state->sq_norm[i][c] += sq(ext[1] - ext[0]);
        }
        
        // rotate back
        if (c > 0) rotate_plane(pset, c - 1);
    }
    
    compute_dct_inplace(pset);
        
    for (uniform int h = 2; h <= state->block_height; h++)
    for (uniform int w = 2; w <= state->block_width; w++)
    {
        uniform int stride = 8;
        uniform int pitch = 64;
        
        float sq_sum = 0;

        for (uniform int y = 0; y < state->block_height; y++)
        for (uniform int x = 0; x < state->block_width; x++)
        {
            if (y < h && x < w) continue;

            for (uniform int p = 0; p < 3; p++)
                sq_sum += sq(pset->pixels[pitch * p + stride * y + x]);
        }

        state->scale_error[h - 2][w - 2] = sq_sum;
    }
}

float estimate_error(astc_rank_state state[], uniform astc_mode mode[])
{
    uniform int c = 0;
    if (mode->dual_plane) c = 1 + mode->color_component_selector;

    float scale_error = state->scale_error[mode->height - 2][mode->width - 2];
    
    uniform bool zero_based = mode->color_endpoint_modes[0] == 6;
    float pca_error = state->pca_error[zero_based][c];
    float sq_norm = state->sq_norm[zero_based][c];

    uniform float sq_rcp_w_levels = get_sq_rcp_levels(mode->weight_range);
    uniform float sq_rcp_ep_levels = get_sq_rcp_levels(mode->endpoint_range);
    float quant_error = 0;

    quant_error += 2.5 * sq_norm * sq_rcp_w_levels;
    quant_error += 9075 * (state->block_height * state->block_width) * sq_rcp_ep_levels;
    
    float error = 0;
    error += scale_error;
    error += pca_error;
    error += quant_error;

    return error;
}

void insert_element(astc_rank_state state[], float error, uint32_t packed_mode, float threshold_error[])
{
    float max_error = 0;

    for (uniform int k = 0; k < state->fastSkipTreshold; k++)
    {
        if (state->best_scores[k] > error)
        {
            swap(state->best_scores[k], error);
            swap(state->best_modes[k], packed_mode);
        }

        max_error = max(max_error, state->best_scores[k]);
    }

    *threshold_error = max_error;
}

uniform static const int packed_modes_count = 1549;
uniform static const uint32_t packed_modes[1549] =
{
    0x00036A00, 0x001369C0, 0x00234A00, 0x00334A01, 0x00434382, 0x00534A40, 0x006346C1, 0x00726A00,
    0x00826A01, 0x00926382, 0x00A26A40, 0x00B266C1, 0x00C33A00, 0x00D33A01, 0x00E33982, 0x00F33403,
    0x01033A40, 0x011339C1, 0x012335C2, 0x01333243, 0x0141EA00, 0x0151EA01, 0x0161E982, 0x0171E403,
    0x0181EA40, 0x0191E9C1, 0x01A1E5C2, 0x01B1E243, 0x01C32A00, 0x01D32A01, 0x01E32A02, 0x01F32A03,
    0x02032804, 0x02132385, 0x02232A40, 0x02332A41, 0x024329C2, 0x02532743, 0x026324C4, 0x02772A00,
    0x02872940, 0x02972A10, 0x02A72950, 0x02B72A20, 0x02C72960, 0x02D16A00, 0x02E16A01, 0x02F16A02,
    0x03016A03, 0x03116804, 0x03216385, 0x03316A40, 0x03416A41, 0x035169C2, 0x03616743, 0x037164C4,
    0x03856A00, 0x03956940, 0x03A56A10, 0x03B56950, 0x03C56A20, 0x03D56960, 0x03E31A00, 0x03F31A01,
    0x04031A02, 0x04131A03, 0x04231A04, 0x04331A05, 0x04431986, 0x04531707, 0x04631388, 0x04731A40,
    0x04831A41, 0x04931A42, 0x04A31A43, 0x04B31A44, 0x04C317C5, 0x04D315C6, 0x04E31447, 0x04F71A00,
    0x05071A01, 0x05171282, 0x05271A40, 0x05371641, 0x05471A10, 0x05571A11, 0x05671292, 0x05771A50,
    0x05871651, 0x05971A20, 0x05A71A21, 0x05B712A2, 0x05C71A60, 0x05D71661, 0x05E0EA00, 0x05F0EA01,
    0x0600EA02, 0x0610EA03, 0x0620EA04, 0x0630EA05, 0x0640E986, 0x0650E707, 0x0660E388, 0x0670EA40,
    0x0680EA41, 0x0690EA42, 0x06A0EA43, 0x06B0EA44, 0x06C0E7C5, 0x06D0E5C6, 0x06E0E447, 0x06F4EA00,
    0x0704EA01, 0x0714E282, 0x0724EA40, 0x0734E641, 0x0744EA10, 0x0754EA11, 0x0764E292, 0x0774EA50,
    0x0784E651, 0x0794EA20, 0x07A4EA21, 0x07B4E2A2, 0x07C4EA60, 0x07D4E661, 0x07E30A01, 0x07F30A02,
    0x08030A03, 0x08130A04, 0x08230A05, 0x08330A06, 0x08430A07, 0x08530A08, 0x08630A09, 0x08730A0A,
    0x0883098B, 0x08930A41, 0x08A30A42, 0x08B30A43, 0x08C30A44, 0x08D30A45, 0x08E30A46, 0x08F30A47,
    0x090309C8, 0x09130849, 0x0923074A, 0x093305CB, 0x09470A00, 0x09570A01, 0x09670A02, 0x09770A03,
    0x09870704, 0x09970285, 0x09A70A40, 0x09B70A41, 0x09C70942, 0x09D706C3, 0x09E70444, 0x09F70A10,
    0x0A070A11, 0x0A170A12, 0x0A270A13, 0x0A370714, 0x0A470295, 0x0A570A50, 0x0A670A51, 0x0A770952,
    0x0A8706D3, 0x0A970454, 0x0AA70A20, 0x0AB70A21, 0x0AC70A22, 0x0AD70A23, 0x0AE70724, 0x0AF702A5,
    0x0B070A60, 0x0B170A61, 0x0B270962, 0x0B3706E3, 0x0B470464, 0x0B506A01, 0x0B606A02, 0x0B706A03,
    0x0B806A04, 0x0B906A05, 0x0BA06A06, 0x0BB06A07, 0x0BC06A08, 0x0BD06A09, 0x0BE06A0A, 0x0BF0698B,
    0x0C006A41, 0x0C106A42, 0x0C206A43, 0x0C306A44, 0x0C406A45, 0x0C506A46, 0x0C606A47, 0x0C7069C8,
    0x0C806849, 0x0C90674A, 0x0CA065CB, 0x0CB46A00, 0x0CC46A01, 0x0CD46A02, 0x0CE46A03, 0x0CF46704,
    0x0D046285, 0x0D146A40, 0x0D246A41, 0x0D346942, 0x0D4466C3, 0x0D546444, 0x0D646A10, 0x0D746A11,
    0x0D846A12, 0x0D946A13, 0x0DA46714, 0x0DB46295, 0x0DC46A50, 0x0DD46A51, 0x0DE46952, 0x0DF466D3,
    0x0E046454, 0x0E146A20, 0x0E246A21, 0x0E346A22, 0x0E446A23, 0x0E546724, 0x0E6462A5, 0x0E746A60,
    0x0E846A61, 0x0E946962, 0x0EA466E3, 0x0EB46464, 0x0EC24A00, 0x0ED24A01, 0x0EE24A02, 0x0EF24803,
    0x0F024404, 0x0F124A40, 0x0F224A41, 0x0F3247C2, 0x0F4244C3, 0x0F524244, 0x0F623A00, 0x0F723A01,
    0x0F823A02, 0x0F923A03, 0x0FA23A04, 0x0FB23585, 0x0FC23A40, 0x0FD23A41, 0x0FE23A42, 0x0FF23843,
    0x10023644, 0x10123345, 0x10263A00, 0x10363281, 0x10463A40, 0x10563A10, 0x10663291, 0x10763A50,
    0x10863A20, 0x109632A1, 0x10A63A60, 0x10B1CA00, 0x10C1CA01, 0x10D1CA02, 0x10E1CA03, 0x10F1CA04,
    0x1101C585, 0x1111CA40, 0x1121CA41, 0x1131CA42, 0x1141C843, 0x1151C644, 0x1161C345, 0x1175CA00,
    0x1185C281, 0x1195CA40, 0x11A5CA10, 0x11B5C291, 0x11C5CA50, 0x11D5CA20, 0x11E5C2A1, 0x11F5CA60,
    0x1201BA00, 0x1211BA01, 0x1221BA02, 0x1231BA03, 0x1241BA04, 0x1251BA05, 0x1261B806, 0x1271B587,
    0x1281BA40, 0x1291BA41, 0x12A1BA42, 0x12B1BA43, 0x12C1B9C4, 0x12D1B745, 0x12E1B4C6, 0x12F1B347,
    0x1305BA00, 0x1315B881, 0x1325BA40, 0x1335B541, 0x1345BA10, 0x1355B891, 0x1365BA50, 0x1375B551,
    0x1385BA20, 0x1395B8A1, 0x13A5BA60, 0x13B5B561, 0x13C22A00, 0x13D22A01, 0x13E22A02, 0x13F22A03,
    0x14022A04, 0x14122A05, 0x14222986, 0x14322707, 0x14422388, 0x14522A40, 0x14622A41, 0x14722A42,
    0x14822A43, 0x14922A44, 0x14A227C5, 0x14B225C6, 0x14C22447, 0x14D62A00, 0x14E62A01, 0x14F62282,
    0x15062A40, 0x15162641, 0x15262A10, 0x15362A11, 0x15462292, 0x15562A50, 0x15662651, 0x15762A20,
    0x15862A21, 0x159622A2, 0x15A62A60, 0x15B62661, 0x15C14A00, 0x15D14A01, 0x15E14A02, 0x15F14A03,
    0x16014A04, 0x16114A05, 0x16214986, 0x16314707, 0x16414388, 0x16514A40, 0x16614A41, 0x16714A42,
    0x16814A43, 0x16914A44, 0x16A147C5, 0x16B145C6, 0x16C14447, 0x16D54A00, 0x16E54A01, 0x16F54282,
    0x17054A40, 0x17154641, 0x17254A10, 0x17354A11, 0x17454292, 0x17554A50, 0x17654651, 0x17754A20,
    0x17854A21, 0x179542A2, 0x17A54A60, 0x17B54661, 0x17C1AA01, 0x17D1AA02, 0x17E1AA03, 0x17F1AA04,
    0x1801AA05, 0x1811AA06, 0x1821AA07, 0x1831A988, 0x1841A709, 0x1851A50A, 0x1861AA41, 0x1871AA42,
    0x1881AA43, 0x1891AA44, 0x18A1AA45, 0x18B1A946, 0x18C1A7C7, 0x18D1A5C8, 0x18E1A449, 0x18F1A2CA,
    0x1905AA00, 0x1915AA01, 0x1925A882, 0x1935A383, 0x1945AA40, 0x1955A941, 0x1965A542, 0x1975AA10,
    0x1985AA11, 0x1995A892, 0x19A5A393, 0x19B5AA50, 0x19C5A951, 0x19D5A552, 0x19E5AA20, 0x19F5AA21,
    0x1A05A8A2, 0x1A15A3A3, 0x1A25AA60, 0x1A35A961, 0x1A45A562, 0x1A513A01, 0x1A613A02, 0x1A713A03,
    0x1A813A04, 0x1A913A05, 0x1AA13A06, 0x1AB13A07, 0x1AC13988, 0x1AD13709, 0x1AE1350A, 0x1AF13A41,
    0x1B013A42, 0x1B113A43, 0x1B213A44, 0x1B313A45, 0x1B413946, 0x1B5137C7, 0x1B6135C8, 0x1B713449,
    0x1B8132CA, 0x1B953A00, 0x1BA53A01, 0x1BB53882, 0x1BC53383, 0x1BD53A40, 0x1BE53941, 0x1BF53542,
    0x1C053A10, 0x1C153A11, 0x1C253892, 0x1C353393, 0x1C453A50, 0x1C553951, 0x1C653552, 0x1C753A20,
    0x1C853A21, 0x1C9538A2, 0x1CA533A3, 0x1CB53A60, 0x1CC53961, 0x1CD53562, 0x1CE12A01, 0x1CF12A02,
    0x1D012A03, 0x1D112A04, 0x1D212A05, 0x1D312A06, 0x1D412A07, 0x1D512A08, 0x1D612A09, 0x1D712A0A,
    0x1D81298B, 0x1D912A41, 0x1DA12A42, 0x1DB12A43, 0x1DC12A44, 0x1DD12A45, 0x1DE12A46, 0x1DF12A47,
    0x1E0129C8, 0x1E112849, 0x1E21274A, 0x1E3125CB, 0x1E452A00, 0x1E552A01, 0x1E652A02, 0x1E752A03,
    0x1E852704, 0x1E952285, 0x1EA52A40, 0x1EB52A41, 0x1EC52942, 0x1ED526C3, 0x1EE52444, 0x1EF52A10,
    0x1F052A11, 0x1F152A12, 0x1F252A13, 0x1F352714, 0x1F452295, 0x1F552A50, 0x1F652A51, 0x1F752952,
    0x1F8526D3, 0x1F952454, 0x1FA52A20, 0x1FB52A21, 0x1FC52A22, 0x1FD52A23, 0x1FE52724, 0x1FF522A5,
    0x20052A60, 0x20152A61, 0x20252962, 0x203526E3, 0x20452464, 0x20521A01, 0x20621A02, 0x20721A03,
    0x20821A04, 0x20921A05, 0x20A21A06, 0x20B21A07, 0x20C21A08, 0x20D21A09, 0x20E2188A, 0x20F2158B,
    0x21021A41, 0x21121A42, 0x21221A43, 0x21321A44, 0x21421A45, 0x21521A46, 0x216219C7, 0x217217C8,
    0x21821649, 0x2192154A, 0x21A2134B, 0x21B61A00, 0x21C61A01, 0x21D61A02, 0x21E61703, 0x21F61384,
    0x22061A40, 0x22161A41, 0x22261742, 0x22361443, 0x22461A10, 0x22561A11, 0x22661A12, 0x22761713,
    0x22861394, 0x22961A50, 0x22A61A51, 0x22B61752, 0x22C61453, 0x22D61A20, 0x22E61A21, 0x22F61A22,
    0x23061723, 0x231613A4, 0x23261A60, 0x23361A61, 0x23461762, 0x23561463, 0x2360CA01, 0x2370CA02,
    0x2380CA03, 0x2390CA04, 0x23A0CA05, 0x23B0CA06, 0x23C0CA07, 0x23D0CA08, 0x23E0CA09, 0x23F0C88A,
    0x2400C58B, 0x2410CA41, 0x2420CA42, 0x2430CA43, 0x2440CA44, 0x2450CA45, 0x2460CA46, 0x2470C9C7,
    0x2480C7C8, 0x2490C649, 0x24A0C54A, 0x24B0C34B, 0x24C4CA00, 0x24D4CA01, 0x24E4CA02, 0x24F4C703,
    0x2504C384, 0x2514CA40, 0x2524CA41, 0x2534C742, 0x2544C443, 0x2554CA10, 0x2564CA11, 0x2574CA12,
    0x2584C713, 0x2594C394, 0x25A4CA50, 0x25B4CA51, 0x25C4C752, 0x25D4C453, 0x25E4CA20, 0x25F4CA21,
    0x2604CA22, 0x2614C723, 0x2624C3A4, 0x2634CA60, 0x2644CA61, 0x2654C762, 0x2664C463, 0x26719A01,
    0x26819A02, 0x26919A03, 0x26A19A04, 0x26B19A05, 0x26C19A06, 0x26D19A07, 0x26E19A08, 0x26F19A09,
    0x27019A0A, 0x27119A0B, 0x27219A41, 0x27319A42, 0x27419A43, 0x27519A44, 0x27619A45, 0x27719A46,
    0x27819A47, 0x27919A48, 0x27A199C9, 0x27B198CA, 0x27C1974B, 0x27D59A00, 0x27E59A01, 0x27F59A02,
    0x28059A03, 0x28159984, 0x28259505, 0x28359A40, 0x28459A41, 0x28559A42, 0x286597C3, 0x287595C4,
    0x288592C5, 0x28959A10, 0x28A59A11, 0x28B59A12, 0x28C59A13, 0x28D59994, 0x28E59515, 0x28F59A50,
    0x29059A51, 0x29159A52, 0x292597D3, 0x293595D4, 0x294592D5, 0x29559A20, 0x29659A21, 0x29759A22,
    0x29859A23, 0x299599A4, 0x29A59525, 0x29B59A60, 0x29C59A61, 0x29D59A62, 0x29E597E3, 0x29F595E4,
    0x2A0592E5, 0x2A10BA01, 0x2A20BA02, 0x2A30BA03, 0x2A40BA04, 0x2A50BA05, 0x2A60BA06, 0x2A70BA07,
    0x2A80BA08, 0x2A90BA09, 0x2AA0BA0A, 0x2AB0BA0B, 0x2AC0BA41, 0x2AD0BA42, 0x2AE0BA43, 0x2AF0BA44,
    0x2B00BA45, 0x2B10BA46, 0x2B20BA47, 0x2B30BA48, 0x2B40B9C9, 0x2B50B8CA, 0x2B60B74B, 0x2B74BA00,
    0x2B84BA01, 0x2B94BA02, 0x2BA4BA03, 0x2BB4B984, 0x2BC4B505, 0x2BD4BA40, 0x2BE4BA41, 0x2BF4BA42,
    0x2C04B7C3, 0x2C14B5C4, 0x2C24B2C5, 0x2C34BA10, 0x2C44BA11, 0x2C54BA12, 0x2C64BA13, 0x2C74B994,
    0x2C84B515, 0x2C94BA50, 0x2CA4BA51, 0x2CB4BA52, 0x2CC4B7D3, 0x2CD4B5D4, 0x2CE4B2D5, 0x2CF4BA20,
    0x2D04BA21, 0x2D14BA22, 0x2D24BA23, 0x2D34B9A4, 0x2D44B525, 0x2D54BA60, 0x2D64BA61, 0x2D74BA62,
    0x2D84B7E3, 0x2D94B5E4, 0x2DA4B2E5, 0x2DB11A02, 0x2DC11A03, 0x2DD11A04, 0x2DE11A05, 0x2DF11A06,
    0x2E011A07, 0x2E111A08, 0x2E211A09, 0x2E311A0A, 0x2E411A0B, 0x2E511A42, 0x2E611A43, 0x2E711A44,
    0x2E811A45, 0x2E911A46, 0x2EA11A47, 0x2EB11A48, 0x2EC11A49, 0x2ED11A4A, 0x2EE11A4B, 0x2EF51A00,
    0x2F051A01, 0x2F151A02, 0x2F251A03, 0x2F351A04, 0x2F451A05, 0x2F551886, 0x2F651607, 0x2F751288,
    0x2F851A40, 0x2F951A41, 0x2FA51A42, 0x2FB51A43, 0x2FC519C4, 0x2FD51745, 0x2FE51546, 0x2FF513C7,
    0x30051A10, 0x30151A11, 0x30251A12, 0x30351A13, 0x30451A14, 0x30551A15, 0x30651896, 0x30751617,
    0x30851298, 0x30951A50, 0x30A51A51, 0x30B51A52, 0x30C51A53, 0x30D519D4, 0x30E51755, 0x30F51556,
    0x310513D7, 0x31151A20, 0x31251A21, 0x31351A22, 0x31451A23, 0x31551A24, 0x31651A25, 0x317518A6,
    0x31851627, 0x319512A8, 0x31A51A60, 0x31B51A61, 0x31C51A62, 0x31D51A63, 0x31E519E4, 0x31F51765,
    0x32051566, 0x321513E7, 0x3220AA02, 0x3230AA03, 0x3240AA04, 0x3250AA05, 0x3260AA06, 0x3270AA07,
    0x3280AA08, 0x3290AA09, 0x32A0AA0A, 0x32B0AA0B, 0x32C0AA42, 0x32D0AA43, 0x32E0AA44, 0x32F0AA45,
    0x3300AA46, 0x3310AA47, 0x3320AA48, 0x3330AA49, 0x3340AA4A, 0x3350AA4B, 0x3364AA00, 0x3374AA01,
    0x3384AA02, 0x3394AA03, 0x33A4AA04, 0x33B4AA05, 0x33C4A886, 0x33D4A607, 0x33E4A288, 0x33F4AA40,
    0x3404AA41, 0x3414AA42, 0x3424AA43, 0x3434A9C4, 0x3444A745, 0x3454A546, 0x3464A3C7, 0x3474AA10,
    0x3484AA11, 0x3494AA12, 0x34A4AA13, 0x34B4AA14, 0x34C4AA15, 0x34D4A896, 0x34E4A617, 0x34F4A298,
    0x3504AA50, 0x3514AA51, 0x3524AA52, 0x3534AA53, 0x3544A9D4, 0x3554A755, 0x3564A556, 0x3574A3D7,
    0x3584AA20, 0x3594AA21, 0x35A4AA22, 0x35B4AA23, 0x35C4AA24, 0x35D4AA25, 0x35E4A8A6, 0x35F4A627,
    0x3604A2A8, 0x3614AA60, 0x3624AA61, 0x3634AA62, 0x3644AA63, 0x3654A9E4, 0x3664A765, 0x3674A566,
    0x3684A3E7, 0x36909A04, 0x36A09A05, 0x36B09A06, 0x36C09A07, 0x36D09A08, 0x36E09A09, 0x36F09A0A,
    0x37009A0B, 0x37109A44, 0x37209A45, 0x37309A46, 0x37409A47, 0x37509A48, 0x37609A49, 0x37709A4A,
    0x37809A4B, 0x37949A01, 0x37A49A02, 0x37B49A03, 0x37C49A04, 0x37D49A05, 0x37E49A06, 0x37F49A07,
    0x38049A08, 0x38149989, 0x3824978A, 0x3834950B, 0x38449A41, 0x38549A42, 0x38649A43, 0x38749A44,
    0x38849A45, 0x38949A46, 0x38A49947, 0x38B49748, 0x38C495C9, 0x38D494CA, 0x38E492CB, 0x38F49A11,
    0x39049A12, 0x39149A13, 0x39249A14, 0x39349A15, 0x39449A16, 0x39549A17, 0x39649A18, 0x39749999,
    0x3984979A, 0x3994951B, 0x39A49A51, 0x39B49A52, 0x39C49A53, 0x39D49A54, 0x39E49A55, 0x39F49A56,
    0x3A049957, 0x3A149758, 0x3A2495D9, 0x3A3494DA, 0x3A4492DB, 0x3A549A21, 0x3A649A22, 0x3A749A23,
    0x3A849A24, 0x3A949A25, 0x3AA49A26, 0x3AB49A27, 0x3AC49A28, 0x3AD499A9, 0x3AE497AA, 0x3AF4952B,
    0x3B049A61, 0x3B149A62, 0x3B249A63, 0x3B349A64, 0x3B449A65, 0x3B549A66, 0x3B649967, 0x3B749768,
    0x3B8495E9, 0x3B9494EA, 0x3BA492EB, 0x3BB20A02, 0x3BC20A03, 0x3BD20A04, 0x3BE20A05, 0x3BF20A06,
    0x3C020A07, 0x3C120A08, 0x3C220A09, 0x3C320A0A, 0x3C420A0B, 0x3C520A42, 0x3C620A43, 0x3C720A44,
    0x3C820A45, 0x3C920A46, 0x3CA20A47, 0x3CB20A48, 0x3CC20A49, 0x3CD20A4A, 0x3CE20A4B, 0x3CF60A00,
    0x3D060A01, 0x3D160A02, 0x3D260A03, 0x3D360A04, 0x3D460A05, 0x3D560886, 0x3D660607, 0x3D760288,
    0x3D860A40, 0x3D960A41, 0x3DA60A42, 0x3DB60A43, 0x3DC609C4, 0x3DD60745, 0x3DE60546, 0x3DF603C7,
    0x3E060A10, 0x3E160A11, 0x3E260A12, 0x3E360A13, 0x3E460A14, 0x3E560A15, 0x3E660896, 0x3E760617,
    0x3E860298, 0x3E960A50, 0x3EA60A51, 0x3EB60A52, 0x3EC60A53, 0x3ED609D4, 0x3EE60755, 0x3EF60556,
    0x3F0603D7, 0x3F160A20, 0x3F260A21, 0x3F360A22, 0x3F460A23, 0x3F560A24, 0x3F660A25, 0x3F7608A6,
    0x3F860627, 0x3F9602A8, 0x3FA60A60, 0x3FB60A61, 0x3FC60A62, 0x3FD60A63, 0x3FE609E4, 0x3FF60765,
    0x40060566, 0x401603E7, 0x40204A02, 0x40304A03, 0x40404A04, 0x40504A05, 0x40604A06, 0x40704A07,
    0x40804A08, 0x40904A09, 0x40A04A0A, 0x40B04A0B, 0x40C04A42, 0x40D04A43, 0x40E04A44, 0x40F04A45,
    0x41004A46, 0x41104A47, 0x41204A48, 0x41304A49, 0x41404A4A, 0x41504A4B, 0x41644A00, 0x41744A01,
    0x41844A02, 0x41944A03, 0x41A44A04, 0x41B44A05, 0x41C44886, 0x41D44607, 0x41E44288, 0x41F44A40,
    0x42044A41, 0x42144A42, 0x42244A43, 0x423449C4, 0x42444745, 0x42544546, 0x426443C7, 0x42744A10,
    0x42844A11, 0x42944A12, 0x42A44A13, 0x42B44A14, 0x42C44A15, 0x42D44896, 0x42E44617, 0x42F44298,
    0x43044A50, 0x43144A51, 0x43244A52, 0x43344A53, 0x434449D4, 0x43544755, 0x43644556, 0x437443D7,
    0x43844A20, 0x43944A21, 0x43A44A22, 0x43B44A23, 0x43C44A24, 0x43D44A25, 0x43E448A6, 0x43F44627,
    0x440442A8, 0x44144A60, 0x44244A61, 0x44344A62, 0x44444A63, 0x445449E4, 0x44644765, 0x44744566,
    0x448443E7, 0x44918A03, 0x44A18A04, 0x44B18A05, 0x44C18A06, 0x44D18A07, 0x44E18A08, 0x44F18A09,
    0x45018A0A, 0x45118A0B, 0x45218A43, 0x45318A44, 0x45418A45, 0x45518A46, 0x45618A47, 0x45718A48,
    0x45818A49, 0x45918A4A, 0x45A18A4B, 0x45B58A01, 0x45C58A02, 0x45D58A03, 0x45E58A04, 0x45F58A05,
    0x46058A06, 0x46158A07, 0x46258888, 0x46358609, 0x4645840A, 0x46558A41, 0x46658A42, 0x46758A43,
    0x46858A44, 0x46958A45, 0x46A588C6, 0x46B58747, 0x46C58548, 0x46D583C9, 0x46E5824A, 0x46F58A11,
    0x47058A12, 0x47158A13, 0x47258A14, 0x47358A15, 0x47458A16, 0x47558A17, 0x47658898, 0x47758619,
    0x4785841A, 0x47958A51, 0x47A58A52, 0x47B58A53, 0x47C58A54, 0x47D58A55, 0x47E588D6, 0x47F58757,
    0x48058558, 0x481583D9, 0x4825825A, 0x48358A21, 0x48458A22, 0x48558A23, 0x48658A24, 0x48758A25,
    0x48858A26, 0x48958A27, 0x48A588A8, 0x48B58629, 0x48C5842A, 0x48D58A61, 0x48E58A62, 0x48F58A63,
    0x49058A64, 0x49158A65, 0x492588E6, 0x49358767, 0x49458568, 0x495583E9, 0x4965826A, 0x49703A03,
    0x49803A04, 0x49903A05, 0x49A03A06, 0x49B03A07, 0x49C03A08, 0x49D03A09, 0x49E03A0A, 0x49F03A0B,
    0x4A003A43, 0x4A103A44, 0x4A203A45, 0x4A303A46, 0x4A403A47, 0x4A503A48, 0x4A603A49, 0x4A703A4A,
    0x4A803A4B, 0x4A943A01, 0x4AA43A02, 0x4AB43A03, 0x4AC43A04, 0x4AD43A05, 0x4AE43A06, 0x4AF43A07,
    0x4B043888, 0x4B143609, 0x4B24340A, 0x4B343A41, 0x4B443A42, 0x4B543A43, 0x4B643A44, 0x4B743A45,
    0x4B8438C6, 0x4B943747, 0x4BA43548, 0x4BB433C9, 0x4BC4324A, 0x4BD43A11, 0x4BE43A12, 0x4BF43A13,
    0x4C043A14, 0x4C143A15, 0x4C243A16, 0x4C343A17, 0x4C443898, 0x4C543619, 0x4C64341A, 0x4C743A51,
    0x4C843A52, 0x4C943A53, 0x4CA43A54, 0x4CB43A55, 0x4CC438D6, 0x4CD43757, 0x4CE43558, 0x4CF433D9,
    0x4D04325A, 0x4D143A21, 0x4D243A22, 0x4D343A23, 0x4D443A24, 0x4D543A25, 0x4D643A26, 0x4D743A27,
    0x4D8438A8, 0x4D943629, 0x4DA4342A, 0x4DB43A61, 0x4DC43A62, 0x4DD43A63, 0x4DE43A64, 0x4DF43A65,
    0x4E0438E6, 0x4E143767, 0x4E243568, 0x4E3433E9, 0x4E44326A, 0x4E510A05, 0x4E610A06, 0x4E710A07,
    0x4E810A08, 0x4E910A09, 0x4EA10A0A, 0x4EB10A0B, 0x4EC10A45, 0x4ED10A46, 0x4EE10A47, 0x4EF10A48,
    0x4F010A49, 0x4F110A4A, 0x4F210A4B, 0x4F350A01, 0x4F450A02, 0x4F550A03, 0x4F650A04, 0x4F750A05,
    0x4F850A06, 0x4F950A07, 0x4FA50A08, 0x4FB50A09, 0x4FC50A0A, 0x4FD5088B, 0x4FE50A41, 0x4FF50A42,
    0x50050A43, 0x50150A44, 0x50250A45, 0x50350A46, 0x50450A47, 0x50550948, 0x506507C9, 0x507506CA,
    0x5085054B, 0x50950A11, 0x50A50A12, 0x50B50A13, 0x50C50A14, 0x50D50A15, 0x50E50A16, 0x50F50A17,
    0x51050A18, 0x51150A19, 0x51250A1A, 0x5135089B, 0x51450A51, 0x51550A52, 0x51650A53, 0x51750A54,
    0x51850A55, 0x51950A56, 0x51A50A57, 0x51B50958, 0x51C507D9, 0x51D506DA, 0x51E5055B, 0x51F50A21,
    0x52050A22, 0x52150A23, 0x52250A24, 0x52350A25, 0x52450A26, 0x52550A27, 0x52650A28, 0x52750A29,
    0x52850A2A, 0x529508AB, 0x52A50A61, 0x52B50A62, 0x52C50A63, 0x52D50A64, 0x52E50A65, 0x52F50A66,
    0x53050A67, 0x53150968, 0x532507E9, 0x533506EA, 0x5345056B, 0x53502A05, 0x53602A06, 0x53702A07,
    0x53802A08, 0x53902A09, 0x53A02A0A, 0x53B02A0B, 0x53C02A45, 0x53D02A46, 0x53E02A47, 0x53F02A48,
    0x54002A49, 0x54102A4A, 0x54202A4B, 0x54342A01, 0x54442A02, 0x54542A03, 0x54642A04, 0x54742A05,
    0x54842A06, 0x54942A07, 0x54A42A08, 0x54B42A09, 0x54C42A0A, 0x54D4288B, 0x54E42A41, 0x54F42A42,
    0x55042A43, 0x55142A44, 0x55242A45, 0x55342A46, 0x55442A47, 0x55542948, 0x556427C9, 0x557426CA,
    0x5584254B, 0x55942A11, 0x55A42A12, 0x55B42A13, 0x55C42A14, 0x55D42A15, 0x55E42A16, 0x55F42A17,
    0x56042A18, 0x56142A19, 0x56242A1A, 0x5634289B, 0x56442A51, 0x56542A52, 0x56642A53, 0x56742A54,
    0x56842A55, 0x56942A56, 0x56A42A57, 0x56B42958, 0x56C427D9, 0x56D426DA, 0x56E4255B, 0x56F42A21,
    0x57042A22, 0x57142A23, 0x57242A24, 0x57342A25, 0x57442A26, 0x57542A27, 0x57642A28, 0x57742A29,
    0x57842A2A, 0x579428AB, 0x57A42A61, 0x57B42A62, 0x57C42A63, 0x57D42A64, 0x57E42A65, 0x57F42A66,
    0x58042A67, 0x58142968, 0x582427E9, 0x583426EA, 0x5844256B, 0x58508A08, 0x58608A09, 0x58708A0A,
    0x58808A0B, 0x58908A48, 0x58A08A49, 0x58B08A4A, 0x58C08A4B, 0x58D48A02, 0x58E48A03, 0x58F48A04,
    0x59048A05, 0x59148A06, 0x59248A07, 0x59348A08, 0x59448A09, 0x59548A0A, 0x59648A0B, 0x59748A42,
    0x59848A43, 0x59948A44, 0x59A48A45, 0x59B48A46, 0x59C48A47, 0x59D48A48, 0x59E48A49, 0x59F48A4A,
    0x5A048A4B, 0x5A148A12, 0x5A248A13, 0x5A348A14, 0x5A448A15, 0x5A548A16, 0x5A648A17, 0x5A748A18,
    0x5A848A19, 0x5A948A1A, 0x5AA48A1B, 0x5AB48A52, 0x5AC48A53, 0x5AD48A54, 0x5AE48A55, 0x5AF48A56,
    0x5B048A57, 0x5B148A58, 0x5B248A59, 0x5B348A5A, 0x5B448A5B, 0x5B548A22, 0x5B648A23, 0x5B748A24,
    0x5B848A25, 0x5B948A26, 0x5BA48A27, 0x5BB48A28, 0x5BC48A29, 0x5BD48A2A, 0x5BE48A2B, 0x5BF48A62,
    0x5C048A63, 0x5C148A64, 0x5C248A65, 0x5C348A66, 0x5C448A67, 0x5C548A68, 0x5C648A69, 0x5C748A6A,
    0x5C848A6B, 0x5C901A08, 0x5CA01A09, 0x5CB01A0A, 0x5CC01A0B, 0x5CD01A48, 0x5CE01A49, 0x5CF01A4A,
    0x5D001A4B, 0x5D141A02, 0x5D241A03, 0x5D341A04, 0x5D441A05, 0x5D541A06, 0x5D641A07, 0x5D741A08,
    0x5D841A09, 0x5D941A0A, 0x5DA41A0B, 0x5DB41A42, 0x5DC41A43, 0x5DD41A44, 0x5DE41A45, 0x5DF41A46,
    0x5E041A47, 0x5E141A48, 0x5E241A49, 0x5E341A4A, 0x5E441A4B, 0x5E541A12, 0x5E641A13, 0x5E741A14,
    0x5E841A15, 0x5E941A16, 0x5EA41A17, 0x5EB41A18, 0x5EC41A19, 0x5ED41A1A, 0x5EE41A1B, 0x5EF41A52,
    0x5F041A53, 0x5F141A54, 0x5F241A55, 0x5F341A56, 0x5F441A57, 0x5F541A58, 0x5F641A59, 0x5F741A5A,
    0x5F841A5B, 0x5F941A22, 0x5FA41A23, 0x5FB41A24, 0x5FC41A25, 0x5FD41A26, 0x5FE41A27, 0x5FF41A28,
    0x60041A29, 0x60141A2A, 0x60241A2B, 0x60341A62, 0x60441A63, 0x60541A64, 0x60641A65, 0x60741A66,
    0x60841A67, 0x60941A68, 0x60A41A69, 0x60B41A6A, 0x60C41A6B,
};

uniform int get_bits(uniform uint32_t value, uniform int from, uniform int to)
{
    return (value >> from) & ((1 << (to + 1 - from)) - 1);
}

void load_mode_parameters(uniform astc_mode* uniform mode, uniform uint32_t packed_mode)
{    
    mode->width = 2 + get_bits(packed_mode, 12, 14); // 2..8 <= 2^3
    mode->height = 2 + get_bits(packed_mode, 15, 17); // 2..8 <= 2^3
    mode->dual_plane = get_bits(packed_mode, 18, 18); // 0 or 1
    mode->partitions = 1;    

    mode->weight_range = get_bits(packed_mode, 0, 3);  // 0..11 <= 2^4
    mode->color_component_selector = get_bits(packed_mode, 4, 5);  // 0..2 <= 2^2
    mode->partition_id = 0;
    mode->color_endpoint_modes[0] = get_bits(packed_mode, 6, 6) * 2 + 6; // 6 or 8
    mode->color_endpoint_pairs = 1 + (mode->color_endpoint_modes[0] / 4);
    mode->endpoint_range = get_bits(packed_mode, 7, 11); // 0..20 <= 2^5
}

export void astc_rank_ispc(uniform rgba_surface src[], uniform int xx, uniform int yy, uniform uint32_t mode_buffer[], uniform astc_enc_settings settings[])
{
    int tex_width = src->width / settings->block_width;
    if (xx + programIndex >= tex_width) return;

    astc_rank_state _state;
    varying astc_rank_state* uniform state = &_state;

    state->block_width = settings->block_width;
    state->block_height = settings->block_height;
    state->fastSkipTreshold = settings->fastSkipTreshold;
    state->refineIterations = settings->refineIterations;

    assert(state->fastSkipTreshold <= 64);

    load_block_interleaved(state->pixels, src, xx + programIndex, yy, state->block_width, state->block_height);
    clear_alpha(state->pixels, state->block_width, state->block_height);

    compute_metrics(state);

    float threshold_error = 0;
    int count = -1;

    for (uniform int id = 0; id < packed_modes_count; id++)
    {
        uniform uint32_t packed_mode = packed_modes[id];

        uniform astc_mode _mode;
        uniform astc_mode* uniform mode = &_mode;
        load_mode_parameters(mode, packed_mode);

        if (mode->height > state->block_height) continue;
        if (mode->width > state->block_width) continue;

        float error = estimate_error(state, mode);
        count += 1;

        if (count < state->fastSkipTreshold)
        {
            state->best_modes[count] = packed_mode;
            state->best_scores[count] = error;

            threshold_error = max(threshold_error, error);
        }
        else if (error < threshold_error)
        {
            insert_element(state, error, packed_mode, &threshold_error);
        }
    }

    for (uniform int i = 0; i < state->fastSkipTreshold; i++)
    {
        mode_buffer[programCount * i + programIndex] = state->best_modes[i];
    }
}

///////////////////////////////////////////////////////////
//				 ASTC candidate encoding

struct astc_block
{
    uniform int width;
    uniform int height;
    uniform bool dual_plane;
    int weight_range;
    uint8_t weights[64];
    int color_component_selector;

    uniform int partitions;
    int partition_id;
    uniform int color_endpoint_pairs;
    int color_endpoint_modes[4];
    int endpoint_range;
    uint8_t endpoints[18];
};

struct astc_enc_state
{
    float pixels[256];
    float scaled_pixels[256];
    uint32_t data[4];

    // settings
    uniform int block_width;
    uniform int block_height;
    uniform int pitch;

    uniform int fastSkipTreshold;
    uniform int refineIterations;
};

struct astc_enc_context
{
    // uniform parameters
    int width;
    int height;
    bool dual_plane;
    int partitions;
    int color_endpoint_pairs;
};

uniform static const float filter_data[309] =
{
     0.688356,-0.188356, 0.414384, 0.085616, 0.085616, 0.414384,-0.188356, 0.688356,
     0.955516,-0.227273, 0.044484, 0.142349, 0.727273,-0.142349,-0.142349, 0.727273,
     0.142349, 0.044484,-0.227273, 0.955516, 0.600000,-0.200000, 0.400000, 0.000000,
     0.200000, 0.200000, 0.000000, 0.400000,-0.200000, 0.600000, 0.828571,-0.142857,
     0.028571, 0.342857, 0.285714,-0.057143,-0.142857, 0.714286,-0.142857,-0.057143,
     0.285714, 0.342857, 0.028571,-0.142857, 0.828571, 0.985714,-0.252381, 0.080952,
    -0.014286, 0.057143, 1.009524,-0.323810, 0.057143,-0.085714, 0.485714, 0.485714,
    -0.085714, 0.057143,-0.323810, 1.009524, 0.057143,-0.014286, 0.080952,-0.252381,
     0.985714, 0.510753,-0.177419, 0.381720,-0.048387, 0.252688, 0.080645, 0.080645,
     0.252688,-0.048387, 0.381720,-0.177419, 0.510753, 0.754228,-0.194882, 0.052858,
     0.398312, 0.147638,-0.040044,-0.016924, 0.547244,-0.148431,-0.148431, 0.547244,
    -0.016924,-0.040044, 0.147638, 0.398312, 0.052858,-0.194882, 0.754228, 0.921235,
    -0.216677, 0.063615,-0.013072, 0.210040, 0.577804,-0.169641, 0.034858,-0.164122,
     0.798726,-0.053828, 0.011061, 0.011061,-0.053828, 0.798726,-0.164122, 0.034858,
    -0.169641, 0.577804, 0.210040,-0.013072, 0.063615,-0.216677, 0.921235, 0.996932,
    -0.209923, 0.069231,-0.020846, 0.003068, 0.016362, 1.119589,-0.369231, 0.111180,
    -0.016362,-0.035452, 0.240891, 0.800000,-0.240891, 0.035452, 0.035452,-0.240891,
     0.800000, 0.240891,-0.035452,-0.016362, 0.111180,-0.369231, 1.119589, 0.016362,
     0.003068,-0.020846, 0.069231,-0.209923, 0.996932, 0.415909,-0.165909, 0.343182,
    -0.093182, 0.234091, 0.015909, 0.161364, 0.088636, 0.088636, 0.161364, 0.015909,
     0.234091,-0.093182, 0.343182,-0.165909, 0.415909, 0.653807,-0.172170, 0.058458,
     0.395689, 0.040094,-0.013613, 0.189195, 0.209906,-0.071270,-0.068923, 0.422170,
    -0.143341,-0.143341, 0.422170,-0.068923,-0.071270, 0.209906, 0.189195,-0.013613,
     0.040094, 0.395689, 0.058458,-0.172170, 0.653807, 0.805363,-0.204713, 0.061406,
    -0.016387, 0.363455, 0.220460,-0.066129, 0.017647,-0.078453, 0.645632,-0.193664,
     0.051682,-0.121551, 0.455481, 0.081527,-0.021756,-0.021756, 0.081527, 0.455481,
    -0.121551, 0.051682,-0.193664, 0.645632,-0.078453, 0.017647,-0.066129, 0.220460,
     0.363455,-0.016387, 0.061406,-0.204713, 0.805363, 0.881593,-0.204539, 0.075065,
    -0.021559, 0.004453, 0.270644, 0.467517,-0.171576, 0.049278,-0.010179,-0.169588,
     0.821023,-0.159819, 0.045902,-0.009481,-0.012311, 0.059603, 0.756331,-0.217226,
     0.044870, 0.044870,-0.217226, 0.756331, 0.059603,-0.012311,-0.009481, 0.045902,
    -0.159819, 0.821023,-0.169588,-0.010179, 0.049278,-0.171576, 0.467517, 0.270644,
     0.004453,-0.021559, 0.075065,-0.204539, 0.881593, 0.967275,-0.287351, 0.076902,
    -0.018670, 0.005432,-0.000959, 0.104719, 0.919524,-0.246087, 0.059743,-0.017382,
     0.003067,-0.127990, 0.653915, 0.300773,-0.073019, 0.021245,-0.003749, 0.064956,
    -0.331864, 1.007366,-0.105833, 0.030792,-0.005434,-0.006723, 0.034349,-0.104266,
     0.996397,-0.289905, 0.051160,-0.005112, 0.026120,-0.079287, 0.323158, 0.571013,
    -0.100767, 0.003834,-0.019590, 0.059465,-0.242368, 0.905074, 0.075575,-0.000959,
     0.004898,-0.014866, 0.060592,-0.226268, 0.981106,
};

uniform static const int filterbank[5][5] =
{
    {   0,   8,  -1,  -1,  -1 },
    {  20,  30,  45,  -1,  -1 },
    {  65,  77,  95, 119,  -1 },
    {  -1,  -1,  -1,  -1,  -1 },
    { 149, 165, 189, 221, 261 },
};

void scale_pixels(astc_enc_state state[], uniform astc_enc_context ctx[])
{
    uniform const float* uniform yfilter = &filter_data[filterbank[state->block_height - 4][ctx->height - 2]];
    uniform const float* uniform xfilter = &filter_data[filterbank[state->block_width - 4][ctx->width - 2]];

    for (uniform int y = 0; y < ctx->height; y++)
    {
        float line[8][3];
        
        if (state->block_height == ctx->height)
        {
            for (uniform int x = 0; x < state->block_width; x++)
            for (uniform int p = 0; p < 3; p++)
                line[x][p] = get_pixel(state->pixels, p, x, y);
        }
        else
        for (uniform int x = 0; x < state->block_width; x++)
        {
            uniform int n = ctx->height;

            for (uniform int p = 0; p < 3; p++) line[x][p] = 0;

            for (uniform int k = 0; k < state->block_height; k++)
            for (uniform int p = 0; p < 3; p++)
                line[x][p] += yfilter[k * n + y] * get_pixel(state->pixels, p, x, k);
        }
        
        if (state->block_width == ctx->width)
        {
            for (uniform int x = 0; x < ctx->width; x++)
            for (uniform int p = 0; p < 3; p++)
                set_pixel(state->scaled_pixels, p, x, y, clamp(line[x][p], 0, 255));
        }
        else
        for (uniform int x = 0; x < ctx->width; x++)
        {
            uniform int n = ctx->width;

            float value[3] = { 0, 0, 0 };

            for (uniform int k = 0; k < state->block_width; k++)
            for (uniform int p = 0; p < 3; p++)
                value[p] += xfilter[k * n + x] * line[k][p];
            
            for (uniform int p = 0; p < 3; p++)
                set_pixel(state->scaled_pixels, p, x, y, clamp(value[p], 0, 255));
        }
    }
}

inline int clamp_unorm8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

inline void apply_blue_contract(int& r, int& g, int& b)
{
    r = (r + b) >> 1;
    g = (g + b) >> 1;
}

void decode_endpoints(float endpoints[8], uint8_t coded_endpoints[], int mode)
{    
    if (mode == 6)
    {
        int v0 = coded_endpoints[0];
        int v1 = coded_endpoints[1];
        int v2 = coded_endpoints[2];
        int v3 = coded_endpoints[3];

        endpoints[0] = (v0 * v3) >> 8;
        endpoints[1] = (v1 * v3) >> 8;
        endpoints[2] = (v2 * v3) >> 8;
        endpoints[3] = 0xFF;

        endpoints[4] = v0;
        endpoints[5] = v1;
        endpoints[6] = v2;
        endpoints[7] = 0xFF;
    }

    if (mode / 4 == 2)
    {
        int v0 = coded_endpoints[0];
        int v1 = coded_endpoints[1];
        int v2 = coded_endpoints[2];
        int v3 = coded_endpoints[3];
        int v4 = coded_endpoints[4];
        int v5 = coded_endpoints[5];

        bool swap_endpoints = v1 + v3 + v5 < v0 + v2 + v4;

        if (swap_endpoints)
        {
            swap(v0, v1);
            swap(v2, v3);
            swap(v4, v5);

            apply_blue_contract(v0, v2, v4);
            apply_blue_contract(v1, v3, v5);
        }        

        endpoints[0] = clamp_unorm8(v0);
        endpoints[1] = clamp_unorm8(v2);
        endpoints[2] = clamp_unorm8(v4);
        endpoints[3] = 0xFF;

        endpoints[4] = clamp_unorm8(v1);
        endpoints[5] = clamp_unorm8(v3);
        endpoints[6] = clamp_unorm8(v5);
        endpoints[7] = 0xFF;
    }
}

void dequant_decode_endpoints(float endpoints[8], uint8_t block_endpoints[], int mode, int range)
{
    int levels = get_levels(range);
    int num_cem_pairs = 1 + mode / 4;

    uint8_t dequant_endpoints[8];
    for (uniform int k = 0; k < 2 * num_cem_pairs; k++)
    {
        dequant_endpoints[k] = (int)((int)block_endpoints[k] * 255.0f / (levels - 1) + 0.5);
    }

    decode_endpoints(endpoints, dequant_endpoints, mode);
}

bool compare_endpoints(uint8_t endpoints[8], astc_block block[])
{
    int sum = 0;
    for (uniform int p = 0; p < 3; p++)
    {
        sum += endpoints[p * 2 + 0];
        sum -= endpoints[p * 2 + 1];
    }
    
    if (-1 <= sum && sum <= 1)
    {
        // avoid being too close so we don't need proper rounding 
        for (uniform int p = 0; p < 3; p++)
        {
            endpoints[p * 2 + 0] = clamp(endpoints[p * 2 + 0] - 1, 0, get_levels(block->endpoint_range) - 1);
        }

        sum = -1;
    }

    return sum > 0;
}

void reorder_endpoints(uint8_t endpoints[8], astc_block block[], bool blue_contract)
{    
    if (compare_endpoints(endpoints, block) == !blue_contract)
    for (uniform int p = 0; p < 4; p++) swap(endpoints[p * 2], endpoints[p * 2 + 1]);
}

inline int quant_endpoint(float value, int levels)
{
    return clamp(value / 255.0f * (levels - 1) + 0.5, 0, levels - 1);
}

void quantize_endpoints_scale(astc_block block[], float endpoints[4])
{
    int ep_levels = get_levels(block->endpoint_range);

    float near[3];
    float far[3];
    for (uniform int p = 0; p < 3; p++)
    {
        near[p] = endpoints[p * 2 + 0];
        far[p] = endpoints[p * 2 + 1];
    }

    for (uniform int p = 0; p < 3; p++)
        block->endpoints[p] = quant_endpoint(far[p], ep_levels);

    float sq_norm = dot3(far, far);
    float scale = dot3(far, near) / sq_norm;

    assert(scale <= 1);
    block->endpoints[3] = quant_endpoint(scale * 256, ep_levels);
}

void quantize_endpoints_pair(astc_block block[], float endpoints[6])
{    
    int ep_levels = get_levels(block->endpoint_range);

    bool blue_contract = true;

    float blue_compressed[6];
    for (uniform int i = 0; i < 2; i++)
    {
        blue_compressed[i + 0] = endpoints[i + 0] * 2 - endpoints[i + 4];
        blue_compressed[i + 2] = endpoints[i + 2] * 2 - endpoints[i + 4];
        blue_compressed[i + 4] = endpoints[i + 4];

        if (blue_compressed[i + 0] < 0) blue_contract = false;
        if (blue_compressed[i + 0] > 255) blue_contract = false;
        if (blue_compressed[i + 2] < 0) blue_contract = false;
        if (blue_compressed[i + 2] > 255) blue_contract = false;
    }

    if (blue_contract)
    {
        for (uniform int p = 0; p < 3; p++)
        {
            block->endpoints[p * 2 + 0] = quant_endpoint(blue_compressed[p * 2 + 0], ep_levels);
            block->endpoints[p * 2 + 1] = quant_endpoint(blue_compressed[p * 2 + 1], ep_levels);
        }
    }
    else
    {
        for (uniform int p = 0; p < 3; p++)
        {
            block->endpoints[p * 2 + 0] = quant_endpoint(endpoints[p * 2 + 0], ep_levels);
            block->endpoints[p * 2 + 1] = quant_endpoint(endpoints[p * 2 + 1], ep_levels);
        }
    }

    reorder_endpoints(block->endpoints, block, blue_contract);
}

void quantize_endpoints(astc_block block[], float endpoints[])
{
    if (block->color_endpoint_modes[0] == 6)
    {
        quantize_endpoints_scale(block, endpoints);
    }
    
    if (block->color_endpoint_modes[0] == 8 || block->color_endpoint_modes[0] == 9)
    {
        quantize_endpoints_pair(block, endpoints);
    }
}

void opt_weights(float scaled_pixels[], astc_block block[])
{
    float rec_endpoints[8];
    dequant_decode_endpoints(rec_endpoints, block->endpoints, block->color_endpoint_modes[0], block->endpoint_range);

    int w_levels = get_levels(block->weight_range);

    float dir[3];
    for (uniform int p = 0; p < 3; p++) dir[p] = rec_endpoints[4 + p] - rec_endpoints[0 + p];
    float sq_norm = dot3(dir, dir);
    for (uniform int p = 0; p < 3; p++) dir[p] *= (w_levels - 1) / sq_norm;

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float pixel[3];
        for (uniform int p = 0; p < 3; p++) pixel[p] = get_pixel(scaled_pixels, p, x, y) - rec_endpoints[0 + p];
        
        int q = clamp(dot3(pixel, dir) + 0.5, 0, w_levels - 1);

        block->weights[y * block->width + x] = q;
    }
}

void sgesv2(float A[4], float x[2], float b[2])
{
    float inv_det = 1.0f / (A[0] * A[3] - A[1] * A[2]);
    x[0] = (b[0] * +A[3] + b[1] * -A[2])*inv_det;
    x[1] = (b[0] * -A[1] + b[1] * +A[0])*inv_det;
}

void ls_refine_scale(float endpoints[4], float scaled_pixels[], astc_block block[])
{
    int levels = get_levels(block->weight_range);
    float levels_rcp = 1.0f / (levels - 1);

    float sum_w = 0;
    float sum_ww = 0;
    float sum_d = 0;
    float sum_wd = 0;

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float w = (int)block->weights[y * block->width + x] * levels_rcp;
        float d = 0;

        for (uniform int p = 0; p < 3; p++) d += sq(get_pixel(scaled_pixels, p, x, y));
        d = sqrt(d+0.1f);

        sum_w += w;
        sum_ww += w*w;
        sum_d += d;
        sum_wd += w*d;
    }

    float sum_1 = 1.0f * block->height * block->width;

    float C[4] = { sum_1, sum_w, sum_w, sum_ww };
    float b[2] = { sum_d, sum_wd };
    float xx[2];

    sgesv2(C, xx, b);

    float scale = xx[0] / (xx[1] + xx[0]);

    if (xx[1] + xx[0] < 1) scale = 1;        
    if (scale > 0.9999) scale = 0.9999;
    if (scale < 0) scale = 0;
        
    float sum_zz = 0;
    float sum_zp[3] = { 0, 0, 0 };
        
    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float w = (int)block->weights[y * block->width + x] * levels_rcp;
        float z = scale + (1 - scale)*w;

        sum_zz += z * z;
        for (uniform int p = 0; p < 3; p++) sum_zp[p] += z * get_pixel(scaled_pixels, p, x, y);
    }

    for (uniform int p = 0; p < 3; p++) endpoints[2 * p + 0] = scale  * sum_zp[p] / sum_zz;
    for (uniform int p = 0; p < 3; p++) endpoints[2 * p + 1] = sum_zp[p] / sum_zz;
}

void ls_refine_pair(float endpoints[6], float scaled_pixels[], astc_block block[])
{
    int levels = get_levels(block->weight_range);
    

    float Atb1[4] = { 0, 0, 0, 0 };
    float sum_q = 0;
    float sum_qq = 0;
    float sum[5] = { 0, 0, 0, 0, 0 };

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        int q = block->weights[y * block->width + x];

        int z = (levels - 1) - q;

        sum_q += q;
        sum_qq += q*q;

        sum[4] += 1;
        for (uniform int p = 0; p < 3; p++) sum[p] += get_pixel(scaled_pixels, p, x, y);
        for (uniform int p = 0; p < 3; p++) Atb1[p] += z * get_pixel(scaled_pixels, p, x, y);
    }

    float Atb2[4];
    for (uniform int p = 0; p < 3; p++)
    {
        Atb2[p] = (levels - 1)*sum[p] - Atb1[p];
    }

    float Cxx = sum[4] * sq(levels - 1) - 2 * (levels - 1)*sum_q + sum_qq;
    float Cyy = sum_qq;
    float Cxy = (levels - 1)*sum_q - sum_qq;
    float scale = 1.0f / (Cxx*Cyy - Cxy*Cxy);

    float ep[8];
    for (uniform int p = 0; p < 3; p++)
    {
        ep[0 + p] = (levels - 1)*(Atb1[p] * Cyy - Atb2[p] * Cxy)*scale;
        ep[4 + p] = (levels - 1)*(Atb2[p] * Cxx - Atb1[p] * Cxy)*scale;
    }

    if (abs(Cxx*Cyy - Cxy*Cxy) < 0.001)
    {
        // flatten
        for (int p = 0; p < 3; p++)
        {
            ep[0 + p] = sum[p] / sum[4];
            ep[4 + p] = ep[0 + p];
        }
    }

    for (uniform int p = 0; p < 3; p++)
    {
        endpoints[2 * p + 0] = ep[0 + p];
        endpoints[2 * p + 1] = ep[4 + p];
    }
}

void ls_refine(float endpoints[], float scaled_pixels[], astc_block block[])
{
    if (block->color_endpoint_modes[0] == 6)
    {
        ls_refine_scale(endpoints, scaled_pixels, block);
    }
    else
    {
        ls_refine_pair(endpoints, scaled_pixels, block);
    }
}

float optimize_alt_plane(uint8_t alt_weights[], float scaled_pixels[], astc_block block[])
{
    int ccs = block->color_component_selector;

    float ext[2] = { 1000, -1000 };

    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float value = get_pixel(scaled_pixels, 3, x, y);
        ext[0] = min(ext[0], value);
        ext[1] = max(ext[1], value);
    }

    float _rec_endpoints[8];
    dequant_decode_endpoints(_rec_endpoints, block->endpoints, block->color_endpoint_modes[0], block->endpoint_range);
    
    float endpoints[8];
    for (int p = 0; p < 3; p++)
    {
        endpoints[p * 2 + 0] = _rec_endpoints[0+p];
        endpoints[p * 2 + 1] = _rec_endpoints[4+p];
    }

    scatter_float(endpoints, ccs * 2 + 0, ext[0]);
    scatter_float(endpoints, ccs * 2 + 1, ext[1]);
    
    quantize_endpoints(block, endpoints);

    float rec_endpoints[8];
    dequant_decode_endpoints(rec_endpoints, block->endpoints, block->color_endpoint_modes[0], block->endpoint_range);

    float base = gather_float(rec_endpoints, 0 + ccs);
    float dir = gather_float(rec_endpoints, 4 + ccs) - base;
    float sq_norm = sq(dir);
    
    int w_levels = get_levels(block->weight_range);
    dir *= (w_levels - 1) / sq_norm;

    float err = 0;
    for (uniform int y = 0; y < block->height; y++)
    for (uniform int x = 0; x < block->width; x++)
    {
        float value = get_pixel(scaled_pixels, 3, x, y) - base;

        int q = clamp(value * dir + 0.5, 0, w_levels - 1);

        alt_weights[y * block->width + x] = q;
    }

    return err;
}

void optimize_block(float scaled_pixels[], astc_block block[], astc_enc_state state[])
{
    pixel_set pset;
    pset.pixels = scaled_pixels;
    pset.width = block->width;
    pset.height = block->height;

    float ep[6];
    bool zero_based = block->color_endpoint_modes[0] == 6;
    compute_pca_endpoints(ep, &pset, zero_based);

    quantize_endpoints(block, ep);
    opt_weights(scaled_pixels, block);
    
    for (uniform int i = 0; i < state->refineIterations; i++)
    {
        ls_refine(ep, scaled_pixels, block);
        quantize_endpoints(block, ep);
        opt_weights(scaled_pixels, block);
    }
    
    if (block->dual_plane)
    {
        uint8_t alt_weights[64];
        optimize_alt_plane(alt_weights, scaled_pixels, block);

        uint8_t block_weights[64];
        for (uniform int i = 0; i < block->width * block->height; i++)
        {
            block_weights[i] = block->weights[i];
        }

        for (uniform int i = 0; i < block->width * block->height; i++)
        {
            block->weights[i * 2 + 0] = block_weights[i];
            block->weights[i * 2 + 1] = alt_weights[i];
        }
    }
}

float measure_error(astc_block block[], astc_enc_state state[])
{
    uniform int pitch = state->block_height * state->block_width;
    assert(pitch <= 64);

    // dequant values    
    uniform int num_weights = block->width * block->height * (block->dual_plane ? 2 : 1);

    range_values weight_range_values = get_range_values(block->weight_range);

    int block_weights[64];
    for (int i = 0; i < num_weights; i++)
    {
        block_weights[i] = ((int)block->weights[i] * 64.0f / (weight_range_values.levels - 1) + 0.5);
    }

    float rgba_endpoints[8];
    dequant_decode_endpoints(rgba_endpoints, block->endpoints, block->color_endpoint_modes[0], block->endpoint_range);
    
    uniform int stride = block->width;
    uniform int Ds = (1024 + state->block_width / 2) / (state->block_width - 1);
    uniform int Dt = (1024 + state->block_height / 2) / (state->block_height - 1);

    uint8_t main_weights[64];
    uint8_t alt_weights[64];

    for (uniform int i = 0; i < num_weights; i++) main_weights[i] = block_weights[i];

    if (block->dual_plane)
    for (uniform int i = 0; i < num_weights/2; i++)
    {
        main_weights[i] = block_weights[i * 2 + 0];
        alt_weights[i] = block_weights[i * 2 + 1];
    }

    float sq_error = 0;

    for (uniform int y = 0; y < state->block_height; y++)
    for (uniform int x = 0; x < state->block_width; x++)
    {
        uniform int gs = (x * Ds * (block->width  - 1) + 32) >> 6;
        uniform int gt = (y * Dt * (block->height - 1) + 32) >> 6;

        uniform int js = gs >> 4;
        uniform int jt = gt >> 4;

        uniform int fs = gs & 0x0F;
        uniform int ft = gt & 0x0F;
        uniform int w11 = ((fs*ft + 8) >> 4);

        int filled_weight = 0;
        int alt_filled_weight = 0;

        {
            int acc = 0;
            acc += main_weights[stride * (jt + 0) + js + 0] * (16 - ft - fs + w11);
            acc += main_weights[stride * (jt + 0) + js + 1] * (fs - w11);
            acc += main_weights[stride * (jt + 1) + js + 0] * (ft - w11);
            acc += main_weights[stride * (jt + 1) + js + 1] * w11;
            filled_weight = (acc + 8) >> 4;
        }


        if (block->dual_plane)
        {
            int acc = 0;
            acc += alt_weights[stride * (jt + 0) + js + 0] * (16 - ft - fs + w11);
            acc += alt_weights[stride * (jt + 0) + js + 1] * (fs - w11);
            acc += alt_weights[stride * (jt + 1) + js + 0] * (ft - w11);
            acc += alt_weights[stride * (jt + 1) + js + 1] * w11;

            alt_filled_weight = (acc + 8) >> 4;
        }

        for (uniform int p = 0; p < 3; p++)
        {
            int C0 = rgba_endpoints[0 + p] * 256 + 128;
            int C1 = rgba_endpoints[4 + p] * 256 + 128;
            int w = filled_weight;

            if (block->dual_plane && block->color_component_selector == p)
            {
                w = alt_filled_weight;
            }

            int C = (C0 * (64 - w) + C1 * w + 32) / 64;

            int diff = (C >> 8) - get_pixel(state->pixels, p, x, y);
            sq_error += diff * diff;
        }
    }

    return sq_error;
}

int code_value(int value, range_values range)
{
    int coded = value;

    if (range.levels_m != 2 && range.levels > 5)
    {
        int value2 = value;
        if (value >= range.levels / 2) value2 = (range.levels - 1) - value;
        int q = (value2 * range.levels_m_rcp) >> 16;
        int r = value2 - q * range.levels_m;
        coded = q + r * (1 << (range.levels_e - 1));
        coded = coded * 2 + ((value >= range.levels / 2) ? 1 : 0);
    }

    return coded;
}

void code_block(astc_block block[])
{
    uniform int num_weights = block->width * block->height * (block->dual_plane ? 2 : 1);

    range_values weight_range_values = get_range_values(block->weight_range);
    for (uniform int i = 0; i < num_weights; i++)
    {
        block->weights[i] = code_value(block->weights[i], weight_range_values);
    }

    range_values endpoint_range_values = get_range_values(block->endpoint_range);
    for (uniform int i = 0; i < 2 * block->color_endpoint_pairs; i++)
    {
        block->endpoints[i] = code_value(block->endpoints[i], endpoint_range_values);
    }
}

extern "C" void pack_block_c(uniform uint32_t data[4], uniform astc_block block[]);

void pack_block(astc_block block[], astc_enc_state state[])
{
    code_block(block);

    foreach_active (instance) 
    {
        uniform astc_block ublock;

        ublock.width = block->width;
        ublock.height = block->height;
        ublock.dual_plane = block->dual_plane;
        ublock.partitions = block->partitions;
        ublock.color_endpoint_pairs = block->color_endpoint_pairs;

        ublock.weight_range = extract(block->weight_range, instance);
        ublock.color_component_selector = extract(block->color_component_selector, instance);
        ublock.partition_id = extract(block->partition_id, instance);
        ublock.endpoint_range = extract(block->endpoint_range, instance);
        ublock.color_endpoint_modes[0] = extract(block->color_endpoint_modes[0], instance);

        uniform int num_weights = block->width * block->height * (block->dual_plane ? 2 : 1);
        for (uniform int i = 0; i < num_weights; i++)
            ublock.weights[i] = extract(block->weights[i], instance);

        for (uniform int i = 0; i < 8; i++)
            ublock.endpoints[i] = extract(block->endpoints[i], instance);

        uniform uint32_t data[4];
        pack_block_c(data, &ublock);
        
        for (uniform int i = 0; i < 4; i++) state->data[i] = insert(state->data[i], instance, data[i]);
    }
}

int get_bits(uint32_t value, uniform int from, uniform int to)
{
    return (value >> from) & ((1 << (to + 1 - from)) - 1);
}

void load_block_parameters(astc_block block[], uint32_t mode, uniform astc_enc_context ctx[])
{
    // uniform parameters
    block->width = ctx->width;
    block->height = ctx->height;
    block->dual_plane = ctx->dual_plane;
    block->partitions = ctx->partitions;
    block->color_endpoint_pairs = ctx->color_endpoint_pairs;

    // varying parameters
    block->weight_range = get_bits(mode, 0, 3);  // 0..11 <= 2^4
    block->color_component_selector = get_bits(mode, 4, 5);  // 0..2 <= 2^2 
    block->partition_id = 0;
    block->color_endpoint_modes[0] = get_bits(mode, 6, 6) * 2 + 6; // 4 or 8
    block->endpoint_range = get_bits(mode, 7, 11); // 0..20 <= 2^5
}

export void astc_encode_ispc(uniform rgba_surface src[], uniform float block_scores[], uniform uint8_t dst[], uniform uint64_t list[], uniform astc_enc_context list_context[], uniform astc_enc_settings settings[])
{
    uint64_t entry = list[programIndex];
    uint32_t offset = entry >> 32;
    uint32_t mode = (entry & 0xFFFFFFFF);
    if (mode == 0) return;
    int yy = offset >> 16;
    int xx = offset & 0xFFFF;

    int tex_width = src->width / settings->block_width;

    astc_enc_state _state;
    varying astc_enc_state* uniform state = &_state;

    state->block_width = settings->block_width;
    state->block_height = settings->block_height;
    state->fastSkipTreshold = settings->fastSkipTreshold;
    state->refineIterations = settings->refineIterations;

    load_block_interleaved(state->pixels, src, xx, yy, state->block_width, state->block_height);

    astc_block _block;
    varying astc_block* uniform block = &_block;

    load_block_parameters(block, mode, list_context);
    
    scale_pixels(state, list_context);
    clear_alpha(state->scaled_pixels, block->width, block->height);
    if (block->dual_plane)
    {
        pixel_set pset;
        pset.pixels = state->scaled_pixels;
        pset.width = block->width;
        pset.height = block->height;

        rotate_plane(&pset, block->color_component_selector);
    }

    optimize_block(state->scaled_pixels, block, state);
    float error = measure_error(block, state);
    
    if (error < gather_float(block_scores, yy * tex_width + xx))
    {
        pack_block(block, state);

        scatter_float(block_scores, yy * tex_width + xx, error);

        for (uniform int i = 0; i < 4; i++)
            scatter_uint((uint32_t*)dst, (yy * tex_width + xx) * 4 + i, state->data[i]);
    }
}
