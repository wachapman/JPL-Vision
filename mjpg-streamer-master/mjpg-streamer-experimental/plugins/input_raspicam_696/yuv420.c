/*
 * Copyright (C) 2017 by Daniel Clouse.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful, 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "yuv420.h"

Yuv_File yuv420_open_read(const char* filename) {
    Yuv_File file;
    file.cols = 0;
    file.rows = 0;
    file.fp = fopen(filename, "rb");
    if (file.fp == NULL) return file;
#define YUV_HEADER_BYTES 25
    unsigned char header[YUV_HEADER_BYTES];
    size_t items_read = fread(header, 1, YUV_HEADER_BYTES, file.fp);
    if (items_read != YUV_HEADER_BYTES) {
        fclose(file.fp);
        file.fp = NULL;
        file.cols = 0;
        file.rows = 0;
        return file;
    }
    int items = sscanf((const char*)header, "#!YUV420 %u,%u",
                       &file.cols, &file.rows);
    if (items != 2) {
        fclose(file.fp);
        file.fp = NULL;
        file.cols = 0;
        file.rows = 0;
    }
    return file;
}

void yuv420_close(Yuv_File* yuv_fp) {
    fclose(yuv_fp->fp);
}

unsigned char* yuv420_malloc(const Yuv_File* yuv_fp) {
    size_t pixels = yuv_fp->cols * yuv_fp->rows;
    size_t bytes = pixels * 3 / 2;
    return (unsigned char*)malloc(bytes);
}

int yuv420_read_next(const Yuv_File* yuv_fp,
                     unsigned char* buf) {
    size_t pixels = yuv_fp->cols * yuv_fp->rows;
    size_t bytes = pixels * 3 / 2;
    size_t items_read = fread(buf, 1, bytes, yuv_fp->fp);
    //fprintf(stderr, "bytes= %u items_read= %u errno= %d\n", bytes, items_read, errno);
    if (items_read != bytes) return -1;
    return 0;
}


unsigned char* yuv420_read(const char* filename,
                           unsigned int* width_ptr,
                           unsigned int* height_ptr) {
    Yuv_File yuv_file = yuv420_open_read(filename);
    if (yuv420_is_null(&yuv_file)) return NULL;
    unsigned char* buf = yuv420_malloc(&yuv_file);
    int status = yuv420_read_next(&yuv_file, buf);
    yuv420_close(&yuv_file);
    if (status < 0) {
        free(buf);
        return NULL;
    }
    *width_ptr = yuv_file.cols;
    *height_ptr = yuv_file.rows;
    return buf;
}

int yuv420_write(const char* filename,
                 unsigned int width,
                 unsigned int height,
                 unsigned char* buf) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) return -1;
    fprintf(fp, "#!YUV420 %7u,%7u\n", width, height);
    fwrite(buf, 1, width * height * 3 / 2, fp);
    fclose(fp);
    return 0;
}

/* formulas are from https://en.wikipedia.org/wiki/YUV */
static inline unsigned char y_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return (( 66 * r + 129 * g +  25 * b + 128) >> 8) +  16;
}

static inline unsigned char u_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
}

static inline unsigned char v_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
}

int convert_rgb_to_yuv420(unsigned int width,
                          unsigned int height,
                          const unsigned char rgb[],
                          size_t yuv_bytes,
                          unsigned char yuv[]) {
    unsigned int ii;
    unsigned int jj;
    unsigned char* y = yuv;
    unsigned int pixels = width * height;
    unsigned int required_yuv_bytes = pixels + pixels / 2;
    if (yuv_bytes < required_yuv_bytes) return -1;
    unsigned char* u = &yuv[pixels];
    unsigned char* v = &yuv[pixels + pixels / 4];
    for (ii = 0; ii < height; ++ii) {
        for (jj = 0; jj < width; ++jj) {
            unsigned int yno_00 = ii * width + jj;
            unsigned int yno_01 = yno_00 + 1;
            unsigned int yno_10 = yno_00 + width;
            unsigned int yno_11 = yno_10 + 1;
            unsigned short r00 = rgb[yno_00 * 3 + 0];
            unsigned short g00 = rgb[yno_00 * 3 + 1];
            unsigned short b00 = rgb[yno_00 * 3 + 2];
            unsigned short r01 = rgb[yno_01 * 3 + 0];
            unsigned short g01 = rgb[yno_01 * 3 + 1];
            unsigned short b01 = rgb[yno_01 * 3 + 2];
            unsigned short r10 = rgb[yno_10 * 3 + 0];
            unsigned short g10 = rgb[yno_10 * 3 + 1];
            unsigned short b10 = rgb[yno_10 * 3 + 2];
            unsigned short r11 = rgb[yno_11 * 3 + 0];
            unsigned short g11 = rgb[yno_11 * 3 + 1];
            unsigned short b11 = rgb[yno_11 * 3 + 2];

            y[yno_00] = y_from_rgb(r00, g00, b00);
            y[yno_01] = y_from_rgb(r01, g01, b01);
            y[yno_10] = y_from_rgb(r10, g10, b10);
            y[yno_11] = y_from_rgb(r11, g11, b11);

            if (ii % 2 == 0 && jj % 2 == 0) {
                unsigned int uvno = (ii / 2) * (width / 2) + (jj / 2);
                unsigned short u_val = u_from_rgb(r00, g00, b00);
                u_val += u_from_rgb(r01, g01, b01);
                u_val += u_from_rgb(r10, g10, b10);
                u_val += u_from_rgb(r11, g11, b11);
                u[uvno] = (unsigned char)((u_val + 2) / 4);

                unsigned short v_val = v_from_rgb(r00, g00, b00);
                v_val += v_from_rgb(r01, g01, b01);
                v_val += v_from_rgb(r10, g10, b10);
                v_val += v_from_rgb(r11, g11, b11);
                v[uvno] = (unsigned char)((v_val + 2) / 4);
            }
        }
    }
    return required_yuv_bytes;
}


static unsigned char limit255(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return val;
}


void convert_yuv420_to_rgb(unsigned int cols,
                           unsigned int rows,
                           const unsigned char yuv[],
                           unsigned char rgb[]) {
    unsigned int ii;
    unsigned int jj;
    unsigned int pixels = cols * rows;
    for (ii = 0; ii < rows; ++ii) {
        unsigned char* row = &rgb[ii * (3 * cols)];
        for (jj = 0; jj < cols; ++jj) {
            /* formulas are from https://en.wikipedia.org/wiki/YUV */
            int y = yuv[ii * cols + jj];
            int uv_offset = (ii / 2) * (cols / 2) + (jj / 2);
            int u = yuv[pixels + uv_offset];
            int v = yuv[pixels + uv_offset + (pixels / 4)];
            int c = y - 16;
            int d = u - 128;
            int e = v - 128;
            unsigned char r = limit255((298*c + 409*e + 128) >> 8);
            unsigned char g = limit255((298*c - 100*d - 208*e + 128) >> 8);
            unsigned char b = limit255((298*c + 516*d + 128) >> 8);
            row[3 * jj + 0] = r;
            row[3 * jj + 1] = g;
            row[3 * jj + 2] = b;
        }
    }
}

void yuv420_get_pixel(unsigned int cols,
                      unsigned int rows,
                      const unsigned char yuv[],
                      unsigned int x,
                      unsigned int y,
                      unsigned char return_yuv_pixel[3]) {
    if (x >= cols || y >= rows) {
        return_yuv_pixel[0] = 0;
        return_yuv_pixel[1] = 0;
        return_yuv_pixel[2] = 0;
    } else {
        unsigned int pixels = cols * rows;
        unsigned int uv_offset = (y / 2) * (cols / 2) + (x / 2);
        unsigned char u = yuv[pixels + uv_offset];
        unsigned char v = yuv[pixels + uv_offset + (pixels / 4)];
        return_yuv_pixel[0] = yuv[y * cols + x];
        return_yuv_pixel[1] = u;
        return_yuv_pixel[2] = v;
    }
}
