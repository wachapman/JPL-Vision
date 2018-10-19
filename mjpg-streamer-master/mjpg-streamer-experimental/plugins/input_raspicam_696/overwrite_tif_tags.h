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
#include "mmal/mmal.h"

/**
 * @brief Overwrite the existing tiff headers with Team 696 bounding box
 *        information.
 *
 * @param width [in]            The number of columns in the unresized image.
 * @param height [in]           The number of rows in the unresized image.
 * @param vwidth [in]           The number of columns in the resized video
 *                              image.
 * @param vheight [in]          The number of rows in the resized video image.
 * @param bbox_coord_count [in] The number of elements in the bbox_coord array.
 *                              This is truncated down in this routine to fit
 *                              in the available space, and to be a multiple of
 *                              four.
 * @param bbox_coord [in]       The bounding box coordinates.  Each bounding
 *                              box requires 4 coordinates in this order:
 *                              x_min, y_min, x_max, y_max.
 * @param exposure [in]         Camera exposure.
 * @param analog_gain [in]      Camera analog gain.
 * @param digital_gain [in]     Camera digital gain.
 * @param awb_red_gain [in]     Camera auto white balance red gain.
 * @param awb_blue_gain [in]    Camera auto white balance blue gain.
 * @param yuv [in]              A single YUV color value, usually from center
 *                              of image.
 * @param flags [in]            Bit flags.  Used to specify the status of
 *                              exposure mode freeze, udp connection status,
 *                              etc.  This routine is agnotic to the meanings
 *                              of these flags.
 * @param buf [in,out]          Points to the start of the JPEG header.  The
 *                              size of the header is encoded in the header
 *                              itself.  This routine writes over the TIF tags
 *                              portion of the header with the supplied
 *                              bounding box info.
 * @return On success, the number of bbox_coords actually written.  This may
 *         be less than bbox_coord.  On failure, -1.  Failure occurs if
 *         buf does not point to a valid JPEG header, or if there is not
 *         enough space in the header to hold the minimal set of TIF tags.
 */
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
                       unsigned char* buf);
