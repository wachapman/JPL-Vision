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
#include <assert.h>

/**
 * @brief Convert a floaing point number to unsigned char.
 *
 * The input value is rounded and limited to the range 0 .. 255.
 */
static unsigned char limit(float in) {
    int rounded = (int)(in + 0.5);
    if (rounded < 0) return 0;
    if (rounded > 255) return 255;
    return (unsigned char) rounded;
}

void yuv_color_space_image(const unsigned int cols,
                           const unsigned int rows,
                           const unsigned char y,
                           unsigned char* yuv) {
    unsigned int ii;
    unsigned int jj;
    assert(cols % 2 == 0);
    assert(rows % 2 == 0);
    const unsigned int pixels_per_side = (cols < rows) ? cols : rows;

    /* Set Y values to the given y value in a
       pixels_per_side x pixels_per_side square.
       In the area outside of that square Y values are set to 0. */

    for (ii = 0; ii < pixels_per_side; ++ii) {
        int y_offset = ii * cols;
        unsigned char* y_row = &yuv[y_offset];
        for (jj = 0; jj < pixels_per_side; ++jj) {
            y_row[jj] = y;
        }
        while (jj < cols) {
            y_row[jj] = 0;
            ++jj;
        }
    }
    while (ii < rows) {
        int y_offset = ii * cols;
        unsigned char* y_row = &yuv[y_offset];
        for (jj = 0; jj < cols; ++jj) {
            y_row[jj] = 0;
        }
        ++ii;
    }


    /* Set U and V values.  U values run 0 .. 255 increasing left to right.
       V values run 0 .. 255 increasing bottom to top.   Anything outside of
       pixels_per_side x pixels_per_side square is set to (128, 128). */

    unsigned int ii2;
    unsigned int jj2;
    float steps_per_pixel = 256.0 / pixels_per_side;
    for (ii2 = 0; ii2 < pixels_per_side / 2; ++ii2) {
        int u_offset = cols * rows + ii2 * (cols / 2);
        int v_offset = u_offset + cols * rows / 4;
        unsigned char* u_row = &yuv[u_offset];
        unsigned char* v_row = &yuv[v_offset];
        for (jj2 = 0; jj2 < pixels_per_side / 2; ++jj2) {
            u_row[jj2] = limit(2 * jj2 * steps_per_pixel);
            v_row[jj2] = limit((pixels_per_side - 2 * ii2) * steps_per_pixel);
        }
        while (jj2 < cols / 2) {
            u_row[jj2] = 128;
            v_row[jj2] = 128;
            ++jj2;
        }
    }
    while (ii2 < rows / 2) {
        int u_offset = cols * rows + ii2 * (cols / 2);
        int v_offset = u_offset + cols * rows / 4;
        unsigned char* u_row = &yuv[u_offset];
        unsigned char* v_row = &yuv[v_offset];
        for (jj2 = 0; jj2 < cols / 2; ++jj2) {
            u_row[jj2] = 128;
            v_row[jj2] = 128;
        }
        ++ii2;
    }
}
