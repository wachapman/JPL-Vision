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
#include "overwrite_tif_tags.h"

/**
 * @brief Write out a float value to addr in network byte order.
 */
static void copy_float_to_nbo(float gain, unsigned char* addr) {
    // Convert float to unsigned int.
    unsigned int* pint = (unsigned int*)&gain;

    // Write out bytes in network byte order.
    *(addr++) = (*pint & 0xff000000) >> 24;
    *(addr++) = (*pint & 0x00ff0000) >> 16;
    *(addr++) = (*pint & 0x0000ff00) >> 8;
    *(addr++) = (*pint & 0x000000ff);
}

int overwrite_tif_tags(unsigned int width,
                       unsigned int height,
                       unsigned int vwidth,
                       unsigned int vheight,
                       unsigned short bbox_coord_count, 
                       const unsigned short bbox_coord[],
                       unsigned int exposure,
                       float analog_gain,
                       float digital_gain,
                       float awb_red_gain,
                       float awb_blue_gain,
                       const unsigned char yuv[3],
                       unsigned char flags,
                       unsigned char* buf) {

    /* Check the 1st four bytes to make sure this is a JPEG header. */

    if (buf[0] != 0xff ||
        buf[1] != 0xd8 ||
        buf[2] != 0xff ||
        buf[3] != 0xe1) return -1;

    /* Read the number of bytes in the remainder of the JPEG header from bytes
       4 and 5. */

    const unsigned int SIZE_FIELD_OFFSET = 4;
    unsigned int total_header_bytes = (((unsigned int)buf[4] << 8) | buf[5]) +
                                      SIZE_FIELD_OFFSET;

    /* Byte points to the start of the TIF tags.  All the offsets in the TIF
       tags portion of the header are indexed from here. */

    const unsigned int TIFF_TAGS_OFFSET = 12;
    unsigned char* byte = &buf[TIFF_TAGS_OFFSET];

    /* Byte count is the current space available in the TIF tags portion of
       the header. */

    unsigned short byte_count = total_header_bytes - TIFF_TAGS_OFFSET;

    const unsigned short BYTE_OFFSET_OF_GAIN_DATA = 62;
    const unsigned short SIZE_OF_GAIN_DATA = 4 * sizeof(float);
    const unsigned short BYTE_OFFSET_OF_BBOX_DATA =
                                BYTE_OFFSET_OF_GAIN_DATA + SIZE_OF_GAIN_DATA;
    const unsigned short BYTES_PER_COORD = sizeof(bbox_coord[0]);
    int max_bbox_data_bytes = byte_count - BYTE_OFFSET_OF_BBOX_DATA;
    if (max_bbox_data_bytes < 0) return -1;
    unsigned short max_coords = max_bbox_data_bytes / BYTES_PER_COORD;
    if (bbox_coord_count > max_coords) bbox_coord_count = max_coords;
    // Bbox_coord_count should be a multiple of 4.
    bbox_coord_count -= bbox_coord_count % 4;

    // Motorola byte order

    byte[0] = 'M';
    byte[1] = 'M';

    // TIF version 42

    byte[2] = 0x00;
    byte[3] = 0x2a;

    // Offset of start of IFD table

    byte[4] = 0x00;
    byte[5] = 0x00;
    byte[6] = 0x00;
    byte[7] = 0x08;

    // Count of IFDs

    byte[8] = 0x00;
    byte[9] = 0x04;

    // IFD 0: Exposure

    byte[10] = 0x96;  // tag (2 bytes)
    byte[11] = 0x97;
    byte[12] = 0x00;  // type = unsigned ints (2 bytes)
    byte[13] = 0x04;
    byte[14] = 0x00;  // count of unsigned ints in data (4 bytes)
    byte[15] = 0x00;
    byte[16] = 0x00;
    byte[17] = 0x01;
    byte[18] = (exposure & 0xff000000) >> 24;  // column count (4 bytes)
    byte[19] = (exposure & 0x00ff0000) >> 16;
    byte[20] = (exposure & 0x0000ff00) >> 8;
    byte[21] = (exposure & 0x000000ff);

    // IFD 1: YUV

    byte[22] = 0x96;  // tag (2 bytes)
    byte[23] = 0x99;
    byte[24] = 0x00;  // type = unsigned char (2 bytes)
    byte[25] = 0x01;
    byte[26] = 0x00;  // count of unsigned shorts in data (4 bytes)
    byte[27] = 0x00;
    byte[28] = 0x00;
    byte[29] = 0x04;
    byte[30] = yuv[0];
    byte[31] = yuv[1];
    byte[32] = yuv[2];
    byte[33] = flags;

    // IFD 2: Gains

    byte[34] = 0x96;  // tag (2 bytes)
    byte[35] = 0x98;
    byte[36] = 0x00;  // type = single float (2 bytes)
    byte[37] = 0x0b;
    byte[38] = 0x00;  // count of floats in data (4 bytes)
    byte[39] = 0x00;
    byte[40] = 0x00;
    byte[41] = 0x04;
    byte[42] = 0x00;
    byte[43] = 0x00;
    byte[44] = 0x00;
    byte[45] = BYTE_OFFSET_OF_GAIN_DATA;

    // IFD 3: BBox data

    byte[46] = 0x96;  // tag (2 bytes)
    byte[47] = 0x96;
    byte[48] = 0x00;  // type = unsigned short (2 bytes)
    byte[49] = 0x03;
    byte[50] = 0x00;  // count of unsigned shorts in data (4 bytes)
    byte[51] = 0x00;
    byte[52] = (bbox_coord_count & 0xff00) >> 8;
    byte[53] = (bbox_coord_count & 0x00ff);
    byte[54] = 0x00;  // offset of start of bbox data (4 bytes)
    byte[55] = 0x00;
    byte[56] = 0x00;
    byte[57] = BYTE_OFFSET_OF_BBOX_DATA;

    // End of IFDs marker

    byte[58] = 0x00;
    byte[59] = 0x00;
    byte[60] = 0x00;
    byte[61] = 0x00;

    // Gains

    copy_float_to_nbo(analog_gain, &byte[BYTE_OFFSET_OF_GAIN_DATA]);
    copy_float_to_nbo(digital_gain, &byte[BYTE_OFFSET_OF_GAIN_DATA + 4]);
    copy_float_to_nbo(awb_red_gain, &byte[BYTE_OFFSET_OF_GAIN_DATA + 8]);
    copy_float_to_nbo(awb_blue_gain, &byte[BYTE_OFFSET_OF_GAIN_DATA + 12]);

    // Bbox coords

    float width_ratio = (float)vwidth / width;
    float height_ratio = (float)vheight / height;
    unsigned short ii;
    unsigned char* p = &byte[BYTE_OFFSET_OF_BBOX_DATA];
    for (ii = 0; ii < bbox_coord_count / 2; ++ii) {
        // Convert detect color blobs image coordinates to video image coords.

        unsigned short x = (bbox_coord[2 * ii] * width_ratio + 0.5);
        unsigned short y = (bbox_coord[2 * ii + 1] * height_ratio + 0.5);

        // Output x and y in network byte order.

        *(p++) = (x & 0xff00) >> 8;
        *(p++) = (x & 0x00ff);
        *(p++) = (y & 0xff00) >> 8;
        *(p++) = (y & 0x00ff);
    }

    // Clear unused bytes at end

    while (p < &byte[byte_count]) {
        *(p++) = 0;
    }
    return bbox_coord_count;
}
