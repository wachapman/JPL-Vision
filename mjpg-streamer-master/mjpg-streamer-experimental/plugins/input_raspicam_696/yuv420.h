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

/**
 * @file
 *
 * Functions for reading, and writing YUV420 (.yuv) files, and for converting
 * YUV420 image from/to RGB format in memory.
 *
 * See https://en.wikipedia.org/wiki/YUV for a discussion of the YUV
 * representation.  The section on Y'UV420p is specifically relevant to the
 * representation supported here.
 */
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    FILE* fp;
    unsigned int cols;
    unsigned int rows;
} Yuv_File;


/**
 * Returns true if yuv_fp, returned from yuv420_open_read(), indicates failure.
 *
 * Yuv420_open_read() sets errno to indicate the cause of failure.  See
 * fopen(3).
 */
static inline bool yuv420_is_null(Yuv_File* yuv_fp) {
    return yuv_fp->fp == NULL;
}

/**
 * Returns the width of each YUV420 image in the file pointed to by yuv_fp.
 */
static inline unsigned int yuv420_get_cols(Yuv_File* yuv_fp) {
    return yuv_fp->cols;
}

/**
 * Returns the height of each YUV420 images in the file pointed to by yuv_fp.
 */
static inline unsigned int yuv420_get_rows(Yuv_File* yuv_fp) {
    return yuv_fp->rows;
}

/**
 * Returns the number of bytes required to contain each YUV420 image in yuv_fp.
 */
static inline unsigned int yuv420_get_bytes(Yuv_File* yuv_fp) {
    return yuv_fp->rows * yuv_fp->cols * 3 / 2;
}

/**
 * Open a YUV420 file (.yuv) and return a handle to it.
 */
Yuv_File yuv420_open_read(const char* filename);

/**
 * Close a YUV420 file handle.
 */
void yuv420_close(Yuv_File* yuv_fp);

/**
 * Allocate space for a single image buffer of size specified in yuv_fp.
 *
 * The caller is responsible to free() it.
 */
unsigned char* yuv420_malloc(const Yuv_File* yuv_fp);


/**
 * Read the next image from yuv_fp.
 *
 * A YUV420 file may contain more than one image.  All images in a file are the
 * same size.  This routine returns the next unread image.
 *
 * @param yuv_fp [in]  Points to a Yuv_File returned from yuv420_open_read().
 * @param buf [out]    A buffer of sufficient size to hold a single image read
 *                     from the file (as returned from yuv420_malloc().
 * @return Zero on success, -1 on failure.
 *
 */
int yuv420_read_next(const Yuv_File* yuv_fp,
                     unsigned char* buf);

/**
 * Read the first YUV420 image from the specified filename.
 *
 * @param filename [in]    Path to the .yuv file to read from.
 * @param width_ptr [out]  *Width_ptr returns the number of columns in the
 *                         returned image.
 * @param height_ptr [out] *Height_ptr returns the number of rows in the
 *                         returned image.
 * @return A pointer to allocated memory containing the image, or NULL on
 *         failure.  Caller is responsible to free() the returned memory.
 */
unsigned char* yuv420_read(const char* filename,
                           unsigned int* width_ptr,
                           unsigned int* height_ptr);

/**
 * Write a single YUV420 image to the specified filename.
 *
 * @param filename [in] Path to the .yuv file to write to.
 * @param width [in]    The number of columns in img.
 * @param height [in]   The number of rows in img.
 * @param img [in]      The YUV420 image to write.
 *
 * @return Zero on success; -1 on failure.  The most likely cause of failure
 *         is that filename cannot be opened for writing.
 */
int yuv420_write(const char* filename,
                 unsigned int width,
                 unsigned int height,
                 unsigned char* img);

/**
 * Convert an RGB image to  YUV420 RGB format.
 *
 * @param width [in]             The number of columns in the RGB image.
 * @param height [in]            The number of rows in the RGB image.
 * @param rgb [out]              The RGB image.  This should point to
 *                               width * height bytes.
 * @param yuv_bytes [in]         The number of bytes in the yuv array.  This
 *                               must be at least width * height * 3 / 2.
 * @param yuv [in]               Memory to store the returned YUV420 image.
 */
int convert_rgb_to_yuv420(unsigned int width,
                          unsigned int height,
                          const unsigned char rgb[],
                          size_t yuv_bytes,
                          unsigned char yuv[]);

/**
 * Convert a YUV420 image to RGB format.
 *
 * @param width [in]             The number of columns in the YUV420 image.
 * @param height [in]            The number of rows in the YUV420 image.
 * @param yuv [in]               The YUV420 image.  This should point to
 *                               width * height * 3 / 2 bytes of memory.
 * @param rgb [out]              Memory to store the returned RGB image.
 *                               This should point to width * height bytes.
 */
void convert_yuv420_to_rgb(unsigned int width,
                           unsigned int height,
                           const unsigned char yuv[],
                           unsigned char rgb[]);

/**
 * Return the pixel value at (x ,y) coord within the given YUV420 image.
 *
 * @param width [in]             The number of columns in the YUV420 image.
 * @param height [in]            The number of rows in the YUV420 image.
 * @param yuv [in]               The YUV420 image.  This should point to
 *                               width * height * 3 / 2 bytes of memory.
 * @param x [in]                 X-coordinate of pixel of interest.
 * @param y [in]                 Y-coordinate of pixel of interest.
 * @param return_yuv_pixel [out] Returns the YUV pixel value at (x, y)
 */
void yuv420_get_pixel(unsigned int width,
                      unsigned int height,
                      const unsigned char yuv[],
                      unsigned int x,
                      unsigned int y,
                      unsigned char return_yuv_pixel[3]);
