//
// Created by Haifa Bogdan Adnan on 06/08/2018.
//

#include "../../../common/common.h"

#include "opencl_kernel.h"

string opencl_kernel = R"OCL(
#define ITEMS_PER_SEGMENT               32
#define BLOCK_SIZE_ULONG                128
#define BLOCK_SIZE_UINT                 256
#define MEMSIZE					        1024
#define ARGON2_PREHASH_DIGEST_LENGTH_UINT   16
#define ARGON2_PREHASH_SEED_LENGTH_UINT     18
#define IXIAN_SEED_SIZE_UINT                39

#define ARGON2_BLOCK_SIZE 1024
#define ARGON2_DWORDS_IN_BLOCK (ARGON2_BLOCK_SIZE / 4)

#define BLAKE_SHARED_MEM            480
#define BLAKE_SHARED_MEM_ULONG       60

#define ARGON2_RAW_LENGTH           8

#define ARGON2_TYPE_VALUE               2
#define ARGON2_VERSION                  0x13

#define BLOCK_BYTES	32
#define OUT_BYTES	16

#define G(m, r, i, a, b, c, d) \
do { \
	a = a + b + m[blake2b_sigma[r][2 * i + 0]]; \
	d = rotr64(d ^ a, 32); \
	c = c + d; \
	b = rotr64(b ^ c, 24); \
	a = a + b + m[blake2b_sigma[r][2 * i + 1]]; \
	d = rotr64(d ^ a, 16); \
	c = c + d; \
	b = rotr64(b ^ c, 63); \
} while ((void)0, 0)

#define ROUND(m, t, r, shfl) \
do { \
	G(m, r, t, v0, v1, v2, v3); \
    shfl[t + 4] = v1; \
    shfl[t + 8] = v2; \
    shfl[t + 12] = v3; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
    v1 = shfl[((t + 1) % 4)+ 4]; \
    v2 = shfl[((t + 2) % 4)+ 8]; \
    v3 = shfl[((t + 3) % 4)+ 12]; \
	G(m, r, (t + 4), v0, v1, v2, v3); \
    shfl[((t + 1) % 4)+ 4] = v1; \
    shfl[((t + 2) % 4)+ 8] = v2; \
    shfl[((t + 3) % 4)+ 12] = v3; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
    v1 = shfl[t + 4]; \
    v2 = shfl[t + 8]; \
    v3 = shfl[t + 12]; \
} while ((void)0, 0)

ulong rotr64(ulong x, ulong n)
{
	return rotate(x, 64 - n);
}

__constant ulong blake2b_IV[8] = {
        0x6A09E667F3BCC908, 0xBB67AE8584CAA73B,
        0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
        0x510E527FADE682D1, 0x9B05688C2B3E6C1F,
        0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179
};

__constant uint blake2b_sigma[12][16] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
        {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
        {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
        {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
        {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
        {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
        {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
        {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
        {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
};

void blake2b_compress(__local ulong *h, __local ulong *m, ulong f0, __local ulong *shfl, int thr_id)
{
    ulong v0, v1, v2, v3;

    mem_fence(CLK_LOCAL_MEM_FENCE);

    v0 = h[thr_id];
    v1 = h[thr_id + 4];
    v2 = blake2b_IV[thr_id];
    v3 = blake2b_IV[thr_id + 4];

    if(thr_id == 0) v3 ^= h[8];
    if(thr_id == 1) v3 ^= h[9];
    if(thr_id == 2) v3 ^= f0;

    ROUND(m, thr_id, 0, shfl);
    ROUND(m, thr_id, 1, shfl);
    ROUND(m, thr_id, 2, shfl);
    ROUND(m, thr_id, 3, shfl);
    ROUND(m, thr_id, 4, shfl);
    ROUND(m, thr_id, 5, shfl);
    ROUND(m, thr_id, 6, shfl);
    ROUND(m, thr_id, 7, shfl);
    ROUND(m, thr_id, 8, shfl);
    ROUND(m, thr_id, 9, shfl);
    ROUND(m, thr_id, 10, shfl);
    ROUND(m, thr_id, 11, shfl);

    h[thr_id] ^= v0 ^ v2;
    h[thr_id + 4] ^= v1 ^ v3;
}

void blake2b_incrementCounter(__local ulong *h, int inc)
{
    h[8] += (inc * 4);
    h[9] += (h[8] < (inc * 4));
}

void blake2b_final_global(__global uint *out, int out_len, __local ulong *h, __local uint *buf, int buf_len, __local ulong *shfl, int thr_id)
{
    int left = BLOCK_BYTES - buf_len;
    __local uint *cursor_out_local = buf + buf_len;

    for(int i=0; i < (left >> 2); i++, cursor_out_local += 4) {
        cursor_out_local[thr_id] = 0;
    }

    if(thr_id == 0) {
        for (int i = 0; i < (left % 4); i++) {
            cursor_out_local[i] = 0;
        }
        blake2b_incrementCounter(h, buf_len);
    }

    blake2b_compress(h, (__local ulong *)buf, 0xFFFFFFFFFFFFFFFF, shfl, thr_id);

    __local uint *cursor_in = (__local uint *)h;
    __global uint *cursor_out_global = out;

    for(int i=0; i < (out_len >> 2); i++, cursor_in += 4, cursor_out_global += 4) {
        cursor_out_global[thr_id] = cursor_in[thr_id];
    }

    if(thr_id == 0) {
        for (int i = 0; i < (out_len % 4); i++) {
            cursor_out_global[i] = cursor_in[i];
        }
    }
}

void blake2b_final_local(__local uint *out, int out_len, __local ulong *h, __local uint *buf, int buf_len, __local ulong *shfl, int thr_id)
{
    int left = BLOCK_BYTES - buf_len;
    __local uint *cursor_out = buf + buf_len;

    for(int i=0; i < (left >> 2); i++, cursor_out += 4) {
        cursor_out[thr_id] = 0;
    }

    if(thr_id == 0) {
        for (int i = 0; i < (left % 4); i++) {
            cursor_out[i] = 0;
        }
        blake2b_incrementCounter(h, buf_len);
    }

    blake2b_compress(h, (__local ulong *)buf, 0xFFFFFFFFFFFFFFFF, shfl, thr_id);

    __local uint *cursor_in = (__local uint *)h;
    cursor_out = out;

    for(int i=0; i < (out_len >> 2); i++, cursor_in += 4, cursor_out += 4) {
        cursor_out[thr_id] = cursor_in[thr_id];
    }

    if(thr_id == 0) {
        for (int i = 0; i < (out_len % 4); i++) {
            cursor_out[i] = cursor_in[i];
        }
    }
}

int blake2b_update_global(__global uint *in, int in_len, __local ulong *h, __local uint *buf, int buf_len, __local ulong *shfl, int thr_id)
{
    __global uint *cursor_in = in;
    __local uint *cursor_out = buf + buf_len;

    if (buf_len + in_len > BLOCK_BYTES) {
        int left = BLOCK_BYTES - buf_len;

        for(int i=0; i < (left >> 2); i++, cursor_in += 4, cursor_out += 4) {
            cursor_out[thr_id] = cursor_in[thr_id];
        }

        if(thr_id == 0) {
            for (int i = 0; i < (left % 4); i++) {
                cursor_out[i] = cursor_in[i];
            }
            blake2b_incrementCounter(h, BLOCK_BYTES);
        }

        blake2b_compress(h, (__local ulong *)buf, 0, shfl, thr_id);

        buf_len = 0;

        in_len -= left;
        in += left;

        while (in_len > BLOCK_BYTES) {
            if(thr_id == 0)
                blake2b_incrementCounter(h, BLOCK_BYTES);

            cursor_in = in;
            cursor_out = buf;

            for(int i=0; i < (BLOCK_BYTES / 4); i++, cursor_in += 4, cursor_out += 4) {
                cursor_out[thr_id] = cursor_in[thr_id];
            }

            blake2b_compress(h, (__local ulong *)buf, 0, shfl, thr_id);

            in_len -= BLOCK_BYTES;
            in += BLOCK_BYTES;
        }
    }

    cursor_in = in;
    cursor_out = buf + buf_len;

    for(int i=0; i < (in_len >> 2); i++, cursor_in += 4, cursor_out += 4) {
        cursor_out[thr_id] = cursor_in[thr_id];
    }

    if(thr_id == 0) {
        for (int i = 0; i < (in_len % 4); i++) {
            cursor_out[i] = cursor_in[i];
        }
    }

    return buf_len + in_len;
}

int blake2b_update_local(__local uint *in, int in_len, __local ulong *h, __local uint *buf, int buf_len, __local ulong *shfl, int thr_id)
{
    __local uint *cursor_in = in;
    __local uint *cursor_out = buf + buf_len;

    if (buf_len + in_len > BLOCK_BYTES) {
        int left = BLOCK_BYTES - buf_len;

        for(int i=0; i < (left >> 2); i++, cursor_in += 4, cursor_out += 4) {
            cursor_out[thr_id] = cursor_in[thr_id];
        }

        if(thr_id == 0) {
            for (int i = 0; i < (left % 4); i++) {
                cursor_out[i] = cursor_in[i];
            }
            blake2b_incrementCounter(h, BLOCK_BYTES);
        }

        blake2b_compress(h, (__local ulong *)buf, 0, shfl, thr_id);

        buf_len = 0;

        in_len -= left;
        in += left;

        while (in_len > BLOCK_BYTES) {
            if(thr_id == 0)
                blake2b_incrementCounter(h, BLOCK_BYTES);

            cursor_in = in;
            cursor_out = buf;

            for(int i=0; i < (BLOCK_BYTES / 4); i++, cursor_in += 4, cursor_out += 4) {
                cursor_out[thr_id] = cursor_in[thr_id];
            }

            blake2b_compress(h, (__local ulong *)buf, 0, shfl, thr_id);

            in_len -= BLOCK_BYTES;
            in += BLOCK_BYTES;
        }
    }

    cursor_in = in;
    cursor_out = buf + buf_len;

    for(int i=0; i < (in_len >> 2); i++, cursor_in += 4, cursor_out += 4) {
        cursor_out[thr_id] = cursor_in[thr_id];
    }

    if(thr_id == 0) {
        for (int i = 0; i < (in_len % 4); i++) {
            cursor_out[i] = cursor_in[i];
        }
    }

    return buf_len + in_len;
}

int blake2b_init(__local ulong *h, int out_len, int thr_id)
{
    h[thr_id * 2] = blake2b_IV[thr_id * 2];
    h[thr_id * 2 + 1] = blake2b_IV[thr_id * 2 + 1];

    if(thr_id == 0) {
        h[8] = h[9] = 0;
        h[0] = 0x6A09E667F3BCC908 ^ ((out_len * 4) | (1 << 16) | (1 << 24));
    }

    return 0;
}

void blake2b_digestLong_global(__global uint *out, int out_len,
                       __global uint *in, int in_len,
                       int thr_id, __local ulong* shared)
{
    __local ulong *h = shared;
	__local ulong *shfl = &h[10];
    __local uint *buf = (__local uint *)&shfl[16];
    __local uint *out_buffer = &buf[32];
    int buf_len;

    if(thr_id == 0) buf[0] = (out_len * 4);
    buf_len = 1;

    if (out_len <= OUT_BYTES) {
        blake2b_init(h, out_len, thr_id);
        buf_len = blake2b_update_global(in, in_len, h, buf, buf_len, shfl, thr_id);
        blake2b_final_global(out, out_len, h, buf, buf_len, shfl, thr_id);
    } else {
        __local uint *cursor_in = out_buffer;
        __global uint *cursor_out = out;

        blake2b_init(h, OUT_BYTES, thr_id);
        buf_len = blake2b_update_global(in, in_len, h, buf, buf_len, shfl, thr_id);
        blake2b_final_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);

        for(int i=0; i < (OUT_BYTES / 8); i++, cursor_in += 4, cursor_out += 4) {
            cursor_out[thr_id] = cursor_in[thr_id];
        }

        out += OUT_BYTES / 2;

        int to_produce = out_len - OUT_BYTES / 2;
        while (to_produce > OUT_BYTES) {
            buf_len = blake2b_init(h, OUT_BYTES, thr_id);
            buf_len = blake2b_update_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);
            blake2b_final_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);

            cursor_out = out;
            cursor_in = out_buffer;
            for(int i=0; i < (OUT_BYTES / 8); i++, cursor_in += 4, cursor_out += 4) {
                cursor_out[thr_id] = cursor_in[thr_id];
            }

            out += OUT_BYTES / 2;
            to_produce -= OUT_BYTES / 2;
        }

        buf_len = blake2b_init(h, to_produce, thr_id);
        buf_len = blake2b_update_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);
        blake2b_final_global(out, to_produce, h, buf, buf_len, shfl, thr_id);
    }
}

void blake2b_digestLong_local(__global uint *out, int out_len,
                        __local uint *in, int in_len,
                        int thr_id, __local ulong* shared)
{
    __local ulong *h = shared;
    __local ulong *shfl = &h[10];
    __local uint *buf = (__local uint *)&shfl[16];
    __local uint *out_buffer = &buf[32];
    int buf_len;

    if(thr_id == 0) buf[0] = (out_len * 4);
    buf_len = 1;

    if (out_len <= OUT_BYTES) {
        blake2b_init(h, out_len, thr_id);
        buf_len = blake2b_update_local(in, in_len, h, buf, buf_len, shfl, thr_id);
        blake2b_final_global(out, out_len, h, buf, buf_len, shfl, thr_id);
    } else {
        __local uint *cursor_in = out_buffer;
        __global uint *cursor_out = out;

        blake2b_init(h, OUT_BYTES, thr_id);
        buf_len = blake2b_update_local(in, in_len, h, buf, buf_len, shfl, thr_id);
        blake2b_final_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);

        for(int i=0; i < (OUT_BYTES / 8); i++, cursor_in += 4, cursor_out += 4) {
            cursor_out[thr_id] = cursor_in[thr_id];
        }

        out += OUT_BYTES / 2;

        int to_produce = out_len - OUT_BYTES / 2;
        while (to_produce > OUT_BYTES) {
            buf_len = blake2b_init(h, OUT_BYTES, thr_id);
            buf_len = blake2b_update_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);
            blake2b_final_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);

            cursor_out = out;
            cursor_in = out_buffer;
            for(int i=0; i < (OUT_BYTES / 8); i++, cursor_in += 4, cursor_out += 4) {
                cursor_out[thr_id] = cursor_in[thr_id];
            }

            out += OUT_BYTES / 2;
            to_produce -= OUT_BYTES / 2;
        }

        buf_len = blake2b_init(h, to_produce, thr_id);
        buf_len = blake2b_update_local(out_buffer, OUT_BYTES, h, buf, buf_len, shfl, thr_id);
        blake2b_final_global(out, to_produce, h, buf, buf_len, shfl, thr_id);
    }
}

#define fBlaMka(x, y) ((x) + (y) + 2 * upsample(mul_hi((uint)(x), (uint)(y)), (uint)(x) * (uint)y))

#define COMPUTE \
    a = fBlaMka(a, b);          \
    d = rotate(d ^ a, (ulong)32);      \
    c = fBlaMka(c, d);          \
    b = rotate(b ^ c, (ulong)40);      \
    a = fBlaMka(a, b);          \
    d = rotate(d ^ a, (ulong)48);      \
    c = fBlaMka(c, d);          \
    b = rotate(b ^ c, (ulong)1);

__constant char offsets_round_1[32][4] = {
        { 0, 4, 8, 12 },
        { 1, 5, 9, 13 },
        { 2, 6, 10, 14 },
        { 3, 7, 11, 15 },
        { 16, 20, 24, 28 },
        { 17, 21, 25, 29 },
        { 18, 22, 26, 30 },
        { 19, 23, 27, 31 },
        { 32, 36, 40, 44 },
        { 33, 37, 41, 45 },
        { 34, 38, 42, 46 },
        { 35, 39, 43, 47 },
        { 48, 52, 56, 60 },
        { 49, 53, 57, 61 },
        { 50, 54, 58, 62 },
        { 51, 55, 59, 63 },
        { 64, 68, 72, 76 },
        { 65, 69, 73, 77 },
        { 66, 70, 74, 78 },
        { 67, 71, 75, 79 },
        { 80, 84, 88, 92 },
        { 81, 85, 89, 93 },
        { 82, 86, 90, 94 },
        { 83, 87, 91, 95 },
        { 96, 100, 104, 108 },
        { 97, 101, 105, 109 },
        { 98, 102, 106, 110 },
        { 99, 103, 107, 111 },
        { 112, 116, 120, 124 },
        { 113, 117, 121, 125 },
        { 114, 118, 122, 126 },
        { 115, 119, 123, 127 },
};

__constant char offsets_round_2[32][4] = {
        { 0, 5, 10, 15 },
        { 1, 6, 11, 12 },
        { 2, 7, 8, 13 },
        { 3, 4, 9, 14 },
        { 16, 21, 26, 31 },
        { 17, 22, 27, 28 },
        { 18, 23, 24, 29 },
        { 19, 20, 25, 30 },
        { 32, 37, 42, 47 },
        { 33, 38, 43, 44 },
        { 34, 39, 40, 45 },
        { 35, 36, 41, 46 },
        { 48, 53, 58, 63 },
        { 49, 54, 59, 60 },
        { 50, 55, 56, 61 },
        { 51, 52, 57, 62 },
        { 64, 69, 74, 79 },
        { 65, 70, 75, 76 },
        { 66, 71, 72, 77 },
        { 67, 68, 73, 78 },
        { 80, 85, 90, 95 },
        { 81, 86, 91, 92 },
        { 82, 87, 88, 93 },
        { 83, 84, 89, 94 },
        { 96, 101, 106, 111 },
        { 97, 102, 107, 108 },
        { 98, 103, 104, 109 },
        { 99, 100, 105, 110 },
        { 112, 117, 122, 127 },
        { 113, 118, 123, 124 },
        { 114, 119, 120, 125 },
        { 115, 116, 121, 126 },
};

__constant char offsets_round_3[32][4] = {
        { 0, 32, 64, 96 },
        { 1, 33, 65, 97 },
        { 16, 48, 80, 112 },
        { 17, 49, 81, 113 },
        { 2, 34, 66, 98 },
        { 3, 35, 67, 99 },
        { 18, 50, 82, 114 },
        { 19, 51, 83, 115 },
        { 4, 36, 68, 100 },
        { 5, 37, 69, 101 },
        { 20, 52, 84, 116 },
        { 21, 53, 85, 117 },
        { 6, 38, 70, 102 },
        { 7, 39, 71, 103 },
        { 22, 54, 86, 118 },
        { 23, 55, 87, 119 },
        { 8, 40, 72, 104 },
        { 9, 41, 73, 105 },
        { 24, 56, 88, 120 },
        { 25, 57, 89, 121 },
        { 10, 42, 74, 106 },
        { 11, 43, 75, 107 },
        { 26, 58, 90, 122 },
        { 27, 59, 91, 123 },
        { 12, 44, 76, 108 },
        { 13, 45, 77, 109 },
        { 28, 60, 92, 124 },
        { 29, 61, 93, 125 },
        { 14, 46, 78, 110 },
        { 15, 47, 79, 111 },
        { 30, 62, 94, 126 },
        { 31, 63, 95, 127 },
};

__constant char offsets_round_4[32][4] = {
        { 0, 33, 80, 113 },
        { 1, 48, 81, 96 },
        { 16, 49, 64, 97 },
        { 17, 32, 65, 112 },
        { 2, 35, 82, 115 },
        { 3, 50, 83, 98 },
        { 18, 51, 66, 99 },
        { 19, 34, 67, 114 },
        { 4, 37, 84, 117 },
        { 5, 52, 85, 100 },
        { 20, 53, 68, 101 },
        { 21, 36, 69, 116 },
        { 6, 39, 86, 119 },
        { 7, 54, 87, 102 },
        { 22, 55, 70, 103 },
        { 23, 38, 71, 118 },
        { 8, 41, 88, 121 },
        { 9, 56, 89, 104 },
        { 24, 57, 72, 105 },
        { 25, 40, 73, 120 },
        { 10, 43, 90, 123 },
        { 11, 58, 91, 106 },
        { 26, 59, 74, 107 },
        { 27, 42, 75, 122 },
        { 12, 45, 92, 125 },
        { 13, 60, 93, 108 },
        { 28, 61, 76, 109 },
        { 29, 44, 77, 124 },
        { 14, 47, 94, 127 },
        { 15, 62, 95, 110 },
        { 30, 63, 78, 111 },
        { 31, 46, 79, 126 },
};

#define G1(data) \
{ \
	mem_fence(CLK_LOCAL_MEM_FENCE); \
	a = data[i1_0]; \
	b = data[i1_1]; \
	c = data[i1_2]; \
	d = data[i1_3]; \
	COMPUTE \
	data[i1_1] = b; \
    data[i1_2] = c; \
    data[i1_3] = d; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
}

#define G2(data) \
{ \
	b = data[i2_1]; \
	c = data[i2_2]; \
	d = data[i2_3]; \
	COMPUTE \
	data[i2_0] = a; \
	data[i2_1] = b; \
    data[i2_2] = c; \
    data[i2_3] = d; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
}

#define G3(data) \
{ \
	a = data[i3_0]; \
	b = data[i3_1]; \
	c = data[i3_2]; \
	d = data[i3_3]; \
	COMPUTE \
	data[i3_1] = b; \
    data[i3_2] = c; \
    data[i3_3] = d; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
}

#define G4(data) \
{ \
	b = data[i4_1]; \
	c = data[i4_2]; \
	d = data[i4_3]; \
	COMPUTE \
	data[i4_0] = a; \
	data[i4_1] = b; \
    data[i4_2] = c; \
    data[i4_3] = d; \
    mem_fence(CLK_LOCAL_MEM_FENCE); \
}

__kernel void fill_blocks(__global ulong *chunk_0,
						__global ulong *chunk_1,
						__global ulong *chunk_2,
						__global ulong *chunk_3,
						__global ulong *chunk_4,
						__global ulong *chunk_5,
						__global ulong *seed,
						__global ulong *out,
						__global int *addresses,
						__global int *segments,
						int threads_per_chunk) {
	__local ulong scratchpad[2 * BLOCK_SIZE_ULONG];
	ulong4 tmp;
	ulong a, b, c, d;

	int hash = get_group_id(0);
	int local_id = get_local_id(0);

	int id = local_id % ITEMS_PER_SEGMENT;
	int segment = local_id / ITEMS_PER_SEGMENT;
	int offset = id * 4;

	ulong chunks[6];
	chunks[0] = (ulong)chunk_0;
	chunks[1] = (ulong)chunk_1;
	chunks[2] = (ulong)chunk_2;
	chunks[3] = (ulong)chunk_3;
	chunks[4] = (ulong)chunk_4;
	chunks[5] = (ulong)chunk_5;
	int chunk_index = hash / threads_per_chunk;
	int chunk_offset = hash - chunk_index * threads_per_chunk;
	__global ulong *memory = (__global ulong *)chunks[chunk_index] + chunk_offset * MEMSIZE * BLOCK_SIZE_ULONG;

	int i1_0 = offsets_round_1[id][0];
	int i1_1 = offsets_round_1[id][1];
	int i1_2 = offsets_round_1[id][2];
	int i1_3 = offsets_round_1[id][3];

	int i2_0 = offsets_round_2[id][0];
	int i2_1 = offsets_round_2[id][1];
	int i2_2 = offsets_round_2[id][2];
	int i2_3 = offsets_round_2[id][3];

	int i3_0 = offsets_round_3[id][0];
	int i3_1 = offsets_round_3[id][1];
	int i3_2 = offsets_round_3[id][2];
	int i3_3 = offsets_round_3[id][3];

	int i4_0 = offsets_round_4[id][0];
	int i4_1 = offsets_round_4[id][1];
	int i4_2 = offsets_round_4[id][2];
	int i4_3 = offsets_round_4[id][3];

	__global ulong *out_mem = out + hash * BLOCK_SIZE_ULONG;
	__global ulong *seed_mem = seed + hash * 4 * BLOCK_SIZE_ULONG + segment * 2 * BLOCK_SIZE_ULONG;

	__global ulong *seed_dst = memory + segment * 512 * BLOCK_SIZE_ULONG;

	vstore4(vload4(0, seed_mem + offset), 0, seed_dst + offset);

	seed_mem += BLOCK_SIZE_ULONG;
	seed_dst += BLOCK_SIZE_ULONG;

	vstore4(vload4(0, seed_mem + offset), 0, seed_dst + offset);

	__global ulong *next_block;
	__global ulong *prev_block;
	__global ulong *ref_block;

	__local ulong *state = scratchpad + segment * BLOCK_SIZE_ULONG;

	segments += segment;
	int inc = 126;

	for(int s=0; s<4; s++) {
		int idx = ((s == 0) ? 2 : 0); // index for first slice in each lane is 2

		__global ushort *curr_seg = (__global ushort *)(segments + s * 2);

		ushort addr_start_idx = curr_seg[0];
		ushort prev_blk_idx = curr_seg[1];

		__global short *start_addr = (__global short *)(addresses + addr_start_idx);
		__global short *stop_addr = (__global short *)(addresses + addr_start_idx + inc);
		inc = 128;

		prev_block = memory + prev_blk_idx * BLOCK_SIZE_ULONG;

		tmp = vload4(0, prev_block + offset);
        vstore4(tmp, 0, state + offset);

		ulong4 ref = 0, next = 0;
		ulong4 nextref = 0;

        short addr1 = start_addr[1];
        if(addr1 != -1) {
    		nextref = vload4(0, memory + addr1 * BLOCK_SIZE_ULONG + offset);
        }

		for(; start_addr < stop_addr; start_addr+=2, idx++) {
            addr1 = start_addr[1];
			next_block = memory + start_addr[0] * BLOCK_SIZE_ULONG;

            if(addr1 != -1) {
                ref = nextref;
                if(start_addr + 2 < stop_addr)
                    nextref = vload4(0, memory + start_addr[3] * BLOCK_SIZE_ULONG + offset);
            }
            else {
                ulong pseudo_rand = state[0];

                ulong ref_lane = ((pseudo_rand >> 32)) % 2; // thr_cost
                uint reference_area_size = 0;
                if (segment == ref_lane) {
                    reference_area_size =
                            s * 128 + idx - 1; // seg_length
                } else {
                    reference_area_size =
                            s * 128 + ((idx == 0) ? (-1) : 0);
                }
                ulong relative_position = pseudo_rand & 0xFFFFFFFF;
                relative_position = relative_position * relative_position >> 32;

                relative_position = reference_area_size - 1 -
                                    (reference_area_size * relative_position >> 32);

                addr1 = ref_lane * 512 + relative_position % 512; // lane_length

        		ref = vload4(0, memory + addr1 * BLOCK_SIZE_ULONG + offset);
            }

            tmp ^= ref;

			vstore4(tmp, 0, state + offset);

			G1(state);
			G2(state);
			G3(state);
			G4(state);

			tmp ^= vload4(0, state + offset);

            vstore4(tmp, 0, next_block + offset);
            vstore4(tmp, 0, state + offset);
		}
		barrier(CLK_GLOBAL_MEM_FENCE);
	}

	__global short *out_addr = (__global short *)(addresses + 1020);

	ulong out_data0 = (memory + out_addr[0] * BLOCK_SIZE_ULONG)[local_id];
	ulong out_data1 = (memory + out_addr[0] * BLOCK_SIZE_ULONG)[local_id + 64];

	out_data0 ^= (memory + out_addr[1] * BLOCK_SIZE_ULONG)[local_id];
	out_data1 ^= (memory + out_addr[1] * BLOCK_SIZE_ULONG)[local_id + 64];

	out_mem[local_id] = out_data0;
	out_mem[local_id + 64] = out_data1;
};

__kernel void prehash (
        __global uint *preseed,
        __global uint *seed,
        __local ulong *blake_shared) {

    int hash = get_group_id(0) * 4;
    int id = get_local_id(0); // 64 threads

    int hash_idx = id >> 4;
    hash += hash_idx;
    id = id & 0xF;

    int thr_id = id % 4; // thread id in session
    int session = id / 4; // 4 blake2b hashing session
    int lane = session / 2;  // 2 lanes
    int idx = session % 2; // idx in lane

    __local uint *local_mem = (__local uint *)&blake_shared[(hash_idx * 4 + session) * BLAKE_SHARED_MEM_ULONG];
    __global uint *local_preseed = preseed + hash * IXIAN_SEED_SIZE_UINT;
    __global uint *local_seed = seed + (hash * 4 + session) * BLOCK_SIZE_UINT;

    __local ulong *h = (__local ulong *)&local_mem[20];
	__local ulong *shfl = &h[10];
	__local uint *buf = (__local uint *)&shfl[16];
	__local uint *value = &buf[32];

    int buf_len = blake2b_init(h, ARGON2_PREHASH_DIGEST_LENGTH_UINT, thr_id);
    *value = 2; //lanes
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = 32; //outlen
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = 1024; //m_cost
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = 1; //t_cost
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = ARGON2_VERSION; //version
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = ARGON2_TYPE_VALUE; //type
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    *value = 92; //pw_len
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    buf_len = blake2b_update_global(local_preseed, 23, h, buf, buf_len, shfl, thr_id);
    *value = 64; //salt_len
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    buf_len = blake2b_update_global(local_preseed + 23, 16, h, buf, buf_len, shfl, thr_id);
    *value = 0; //secret_len
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    buf_len = blake2b_update_local(0, 0, h, buf, buf_len, shfl, thr_id);
    *value = 0; //ad_len
    buf_len = blake2b_update_local(value, 1, h, buf, buf_len, shfl, thr_id);
    buf_len = blake2b_update_local(0, 0, h, buf, buf_len, shfl, thr_id);

    blake2b_final_local(local_mem, ARGON2_PREHASH_DIGEST_LENGTH_UINT, h, buf, buf_len, shfl, thr_id);

    if (thr_id == 0) {
        local_mem[ARGON2_PREHASH_DIGEST_LENGTH_UINT] = idx;
        local_mem[ARGON2_PREHASH_DIGEST_LENGTH_UINT + 1] = lane;
    }

    blake2b_digestLong_local(local_seed, ARGON2_DWORDS_IN_BLOCK, local_mem, ARGON2_PREHASH_SEED_LENGTH_UINT, thr_id, (__local ulong *)&local_mem[20]);
}

__kernel void posthash (
        __global uint *hash,
        __global uint *out,
        __local ulong *blake_shared) {

	int hash_id = get_group_id(0);
	int thread = get_local_id(0);

    int thr_id = thread % 4; // thread id in session
    int session = thread / 4; // 16 blake2b hashing session

    __local ulong *local_mem = &blake_shared[session * BLAKE_SHARED_MEM_ULONG];
    __global uint *local_hash = hash + (hash_id * 16 + session) * ARGON2_RAW_LENGTH;
    __global uint *local_out = out + (hash_id * 16 + session) * BLOCK_SIZE_UINT;

    blake2b_digestLong_global(local_hash, ARGON2_RAW_LENGTH, local_out, ARGON2_DWORDS_IN_BLOCK, thr_id, local_mem);
}

)OCL";
