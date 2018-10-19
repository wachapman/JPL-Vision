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
#ifndef TCP_COMMS_H
#define TCP_COMMS_H

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "mmal.h"
#include "RaspiCamControl.h"

typedef struct {
    bool is_connected;
    int fd;
    struct sockaddr_storage saddr;
    socklen_t saddr_len;
} Tcp_Host_Info;

typedef struct {
    RASPICAM_CAMERA_PARAMETERS cam_params;
    bool test_img_enable;
    bool yuv_write;
    bool jpg_write;
    bool detect_yuv;
    unsigned char blob_yuv_min[3];
    unsigned char blob_yuv_max[3];
    float analog_gain_target;
    float analog_gain_tol;
    float digital_gain_target;
    float digital_gain_tol;
    int crosshairs_x;
    int crosshairs_y;
    pthread_mutex_t params_mutex;           /// mutual exclusion lock
} Tcp_Params;

typedef struct {
    Tcp_Host_Info client;
    MMAL_COMPONENT_T* camera_ptr;
    Tcp_Params* params_ptr;
} Connection_Thread_Info;

typedef struct {
    Tcp_Host_Info server;
    MMAL_COMPONENT_T* camera_ptr;
    Tcp_Params* params_ptr;
    int connection_count;
    Connection_Thread_Info* connection;
    pthread_mutex_t connection_mutex;           /// mutual exclusion lock
} Tcp_Comms;

typedef enum { TEXT_COLOR_RED = 'r',
               TEXT_COLOR_GREEN = 'g',
               TEXT_COLOR_BLUE = 'b',
               TEXT_COLOR_MAGENTA = 'm',
               TEXT_COLOR_YELLOW = 'y',
               TEXT_COLOR_CYAN = 'c',
               TEXT_COLOR_ORANGE = 'o' } Text_Color;


#define RASPICAM_QUIT                    0
#define RASPICAM_SATURATION              1
#define RASPICAM_SHARPNESS               2
#define RASPICAM_CONTRAST                3
#define RASPICAM_BRIGHTNESS              4
#define RASPICAM_ISO                     5
#define RASPICAM_METERING_MODE           6
#define RASPICAM_VIDEO_STABILISATION     7
#define RASPICAM_EXPOSURE_COMPENSATION   8
#define RASPICAM_EXPOSURE_MODE           9
#define RASPICAM_AWB_MODE               10
#define RASPICAM_AWB_GAINS              11
#define RASPICAM_IMAGE_FX               12
#define RASPICAM_COLOUR_FX              13
#define RASPICAM_ROTATION               14
#define RASPICAM_FLIPS                  15
#define RASPICAM_ROI                    16
#define RASPICAM_SHUTTER_SPEED          17
#define RASPICAM_DRC                    18
#define RASPICAM_STATS_PASS             19
#define RASPICAM_TEST_IMAGE_ENABLE      20
#define RASPICAM_YUV_WRITE_ENABLE       21
#define RASPICAM_JPG_WRITE_ENABLE       22
#define RASPICAM_DETECT_YUV_ENABLE      23
#define RASPICAM_BLOB_YUV               24
#define RASPICAM_FREEZE_EXPOSURE        25
#define RASPICAM_CROSSHAIRS             26

typedef struct {
    unsigned char tag;
    unsigned char c[12];
} Raspicam_Char_Msg;

typedef struct {
    unsigned char tag;
    unsigned char filler[3];
    int int0;
    int int1;
    int int2;
} Raspicam_Int_Msg;

typedef struct {
    unsigned char tag;
    unsigned char filler[3];
    float float0;
    float float1;
    float float2;
    float float3;
} Raspicam_Float_Msg;


int tcp_params_construct(Tcp_Params* tcp_params_ptr);

int tcp_comms_construct(Tcp_Comms* comms_ptr,
                        MMAL_COMPONENT_T* camera_ptr,
                        Tcp_Params* tcp_params_ptr,
                        unsigned short port_number);

void tcp_comms_send_string(Tcp_Comms* comms_ptr,
                           Text_Color color,
                           const char* string);
#endif
