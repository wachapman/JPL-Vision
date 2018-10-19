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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "tcp_comms.h"
#include "get_ip_addr_str.h"
#include "mmal.h"
#include "get_usecs.h"
#include "mmal/mmal.h"


static inline int_limit(int low, int high, int value) {
    return (value < low) ? low : (value > high) ? high : value;
}

static inline float_limit(float low, float high, float value) {
    return (value < low) ? low : (value > high) ? high : value;
}

static inline float ntohf(float in) {
    uint32_t* p = (uint32_t*)&in;
    uint32_t out = ntohl(*p);
    return *(float*)&out;
}

static inline float htonf(float in) {
    uint32_t* p = (uint32_t*)&in;
    uint32_t out = htonl(*p);
    return *(float*)&out;
}

static inline float htond(double in) {
    int ii;
    double out;
    uint8_t* p_in = (uint8_t*)&in;
    uint8_t* p_out = (uint8_t*)&out;
    for (ii = 0; ii < 8; ++ii) {
        p_out[ii] = p_in[7 - ii];
    }
    return out;
}

static void tcp_params_byte_swap(Tcp_Params* p) {
    p->cam_params.sharpness = htonl(p->cam_params.sharpness);
    p->cam_params.contrast = htonl(p->cam_params.contrast);
    p->cam_params.brightness = htonl(p->cam_params.brightness);
    p->cam_params.saturation = htonl(p->cam_params.saturation);
    p->cam_params.ISO = htonl(p->cam_params.ISO);
    p->cam_params.videoStabilisation = htonl(p->cam_params.videoStabilisation);
    p->cam_params.exposureCompensation =
                                    htonl(p->cam_params.exposureCompensation);
    p->cam_params.exposureMode = htonl(p->cam_params.exposureMode);
    p->cam_params.exposureMeterMode = htonl(p->cam_params.exposureMeterMode);
    p->cam_params.awbMode = htonl(p->cam_params.awbMode);
    p->cam_params.rotation = htonl(p->cam_params.rotation);
    p->cam_params.hflip = htonl(p->cam_params.hflip);
    p->cam_params.vflip = htonl(p->cam_params.vflip);
    p->cam_params.roi.x = htond(p->cam_params.roi.x);
    p->cam_params.roi.y = htond(p->cam_params.roi.y);
    p->cam_params.roi.w = htond(p->cam_params.roi.w);
    p->cam_params.roi.h = htond(p->cam_params.roi.h);
    p->cam_params.shutter_speed = htonl(p->cam_params.shutter_speed);
    p->cam_params.awb_gains_r = htonf(p->cam_params.awb_gains_r);
    p->cam_params.awb_gains_b = htonf(p->cam_params.awb_gains_b);
    p->cam_params.drc_level = htonl(p->cam_params.drc_level);
    p->analog_gain_target = htonf(p->analog_gain_target);
    p->analog_gain_tol = htonf(p->analog_gain_tol);
    p->digital_gain_target = htonf(p->digital_gain_target);
    p->digital_gain_tol = htonf(p->digital_gain_tol);
    p->crosshairs_x = htonl(p->crosshairs_x);
    p->crosshairs_y = htonl(p->crosshairs_y);
}

static void* connection_thread(void* void_args_ptr) {
#define INT1    8
#define INT2   12
#define INT3   16
#define FLOAT1  8
#define FLOAT2 12
#define FLOAT4 20

#define MAX_MESG 64
    unsigned char mesg[MAX_MESG];
    Connection_Thread_Info* args_ptr = (Connection_Thread_Info*)void_args_ptr;
    Tcp_Host_Info* client_ptr = &args_ptr->client;
    MMAL_COMPONENT_T* camera_ptr = args_ptr->camera_ptr;
    Tcp_Params* params_ptr = args_ptr->params_ptr;

#define MAX_CLIENT_STRING 64
    char client_string[MAX_CLIENT_STRING];
    get_ip_addr_str((const struct sockaddr*)&client_ptr->saddr,
                     client_string, MAX_CLIENT_STRING);

    Raspicam_Char_Msg* char_msg_ptr = (Raspicam_Char_Msg*)mesg;
    Raspicam_Int_Msg* int_msg_ptr = (Raspicam_Int_Msg*)mesg;
    Raspicam_Float_Msg* float_msg_ptr = (Raspicam_Float_Msg*)mesg;
    bool error_seen = false;
    ssize_t bytes;
    while ((bytes = recv(client_ptr->fd, &mesg, sizeof(mesg), 0)) > 0) {
        float timestamp = get_usecs() / (float)USECS_PER_SECOND;
        pthread_mutex_lock(&params_ptr->params_mutex);
        switch (mesg[0]) {
        case RASPICAM_QUIT:
            LOG_STATUS("at %.3f, tcp client %s quit\n",
                       timestamp, client_string);
            //free(args_ptr);
            //return NULL;
        case RASPICAM_SATURATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.saturation =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_saturation(
                               camera_ptr, params_ptr->cam_params.saturation);
            }
            break;
        case RASPICAM_SHARPNESS :
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.sharpness =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_sharpness(camera_ptr, 
                                              params_ptr->cam_params.sharpness);
            }
            break;
        case RASPICAM_CONTRAST:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.contrast =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_contrast(camera_ptr, 
                                             params_ptr->cam_params.contrast);
            }
            break;
        case RASPICAM_BRIGHTNESS:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.brightness =
                                   int_limit(0, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_brightness(
                                camera_ptr, params_ptr->cam_params.brightness);
            }
            break;
        case RASPICAM_ISO:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.ISO = ntohl(int_msg_ptr->int0);
                raspicamcontrol_set_ISO(camera_ptr, params_ptr->cam_params.ISO);
            }
            break;
        case RASPICAM_METERING_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureMeterMode =
                                    (MMAL_PARAM_EXPOSUREMETERINGMODE_T)
                                    int_limit(0, 3, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_metering_mode(
                         camera_ptr, params_ptr->cam_params.exposureMeterMode);
            }
            break;
        case RASPICAM_VIDEO_STABILISATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.videoStabilisation =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_video_stabilisation(
                        camera_ptr, params_ptr->cam_params.videoStabilisation);
            }
            break;
        case RASPICAM_EXPOSURE_COMPENSATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureCompensation =
                                   int_limit(-10, 10, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_exposure_compensation(
                      camera_ptr, params_ptr->cam_params.exposureCompensation);
            }
            break;
        case RASPICAM_EXPOSURE_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureMode =
                                    (MMAL_PARAM_EXPOSUREMODE_T)
                                    int_limit(0, 12, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_exposure_mode(
                              camera_ptr, params_ptr->cam_params.exposureMode);
            }
            break;
        case RASPICAM_AWB_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.awbMode = (MMAL_PARAM_AWBMODE_T)
                                    int_limit(0, 9, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_awb_mode(camera_ptr,
                                             params_ptr->cam_params.awbMode);
            }
            break;
        case RASPICAM_AWB_GAINS:
            if (bytes < FLOAT2) {
                error_seen = true;
            } else {
                params_ptr->cam_params.awb_gains_r =
                                                ntohf(float_msg_ptr->float0);
                params_ptr->cam_params.awb_gains_b =
                                                ntohf(float_msg_ptr->float1);
                raspicamcontrol_set_awb_gains(
                                           camera_ptr,
                                           params_ptr->cam_params.awb_gains_r,
                                           params_ptr->cam_params.awb_gains_b);
            }
            break;
        case RASPICAM_IMAGE_FX:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.imageEffect = (MMAL_PARAM_IMAGEFX_T)
                                    int_limit(0, 22, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_imageFX(camera_ptr, 
                                            params_ptr->cam_params.imageEffect);
            }
            break;
        case RASPICAM_COLOUR_FX:
            if (bytes < INT3) {
                error_seen = true;
            } else {
                params_ptr->cam_params.colourEffects.enable =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int0));
                params_ptr->cam_params.colourEffects.u =
                                    int_limit(0, 255, ntohl(int_msg_ptr->int1));
                params_ptr->cam_params.colourEffects.v =
                                    int_limit(0, 255, ntohl(int_msg_ptr->int2));
                raspicamcontrol_set_colourFX(
                             camera_ptr, &params_ptr->cam_params.colourEffects);
            }
            break;
        case RASPICAM_ROTATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.rotation =
                                    int_limit(0, 359, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_rotation(camera_ptr,
                                             params_ptr->cam_params.rotation);
            }
            break;
        case RASPICAM_FLIPS:
            if (bytes < INT2) {
                error_seen = true;
            } else {
                params_ptr->cam_params.hflip =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int0));
                params_ptr->cam_params.vflip =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int1));
                raspicamcontrol_set_flips(camera_ptr,
                                          params_ptr->cam_params.hflip,
                                          params_ptr->cam_params.vflip);
            }
            break;
        case RASPICAM_ROI:
            if (bytes < FLOAT4) {
                error_seen = true;
            } else {
                params_ptr->cam_params.roi.x =
                           float_limit(0.0, 1.0, ntohf(float_msg_ptr->float0));
                params_ptr->cam_params.roi.y =
                           float_limit(0.0, 1.0, ntohf(float_msg_ptr->float1));
                params_ptr->cam_params.roi.w =
                           float_limit(0.0, 1.0, ntohf(float_msg_ptr->float2));
                params_ptr->cam_params.roi.h =
                           float_limit(0.0, 1.0, ntohf(float_msg_ptr->float3));
                raspicamcontrol_set_ROI(camera_ptr, params_ptr->cam_params.roi);
            }
            break;
        case RASPICAM_SHUTTER_SPEED:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.shutter_speed = ntohl(int_msg_ptr->int0);
                raspicamcontrol_set_shutter_speed(
                             camera_ptr, params_ptr->cam_params.shutter_speed);
            }
            break;
        case RASPICAM_DRC:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.drc_level =
                                     (MMAL_PARAMETER_DRC_STRENGTH_T)
                                     int_limit(0, 3, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_DRC(camera_ptr,
                                        params_ptr->cam_params.drc_level);
            }
            break;
        case RASPICAM_STATS_PASS:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.stats_pass =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_stats_pass(
                                camera_ptr, params_ptr->cam_params.stats_pass);
            }
            break;
        case RASPICAM_TEST_IMAGE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->test_img_enable =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_YUV_WRITE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->yuv_write = int_limit(0, 1,
                                                  ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_JPG_WRITE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->jpg_write = int_limit(0, 1,
                                                  ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_DETECT_YUV_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->detect_yuv = int_limit(0, 1,
                                                   ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_BLOB_YUV:
            if (bytes < 7) {
                error_seen = true;
            } else {
                params_ptr->blob_yuv_min[0] = char_msg_ptr->c[0];
                params_ptr->blob_yuv_max[0] = char_msg_ptr->c[1];
                params_ptr->blob_yuv_min[1] = char_msg_ptr->c[2];
                params_ptr->blob_yuv_max[1] = char_msg_ptr->c[3];
                params_ptr->blob_yuv_min[2] = char_msg_ptr->c[4];
                params_ptr->blob_yuv_max[2] = char_msg_ptr->c[5];
            }
            break;
        case RASPICAM_FREEZE_EXPOSURE:
            if (bytes < FLOAT4) {
                error_seen = true;
            } else {
                float f0 = ntohf(float_msg_ptr->float0);
                float f1 = ntohf(float_msg_ptr->float1);
                float f2 = ntohf(float_msg_ptr->float2);
                float f3 = ntohf(float_msg_ptr->float3);
                params_ptr->analog_gain_target = f0;
                params_ptr->analog_gain_tol = f1;
                params_ptr->digital_gain_target = f2;
                params_ptr->digital_gain_tol = f3;
                raspicamcontrol_set_exposure_mode(
                       camera_ptr, params_ptr->cam_params.exposureMode);
            }
            break;
        case RASPICAM_CROSSHAIRS:
            if (bytes < INT2) {
                error_seen = true;
            } else {
                params_ptr->crosshairs_x = ntohl(int_msg_ptr->int0);
                params_ptr->crosshairs_y = ntohl(int_msg_ptr->int1);
            }
            break;
        default:
            LOG_ERROR("at %.3f, unexpected tcp message tag %d from %s\n",
                      timestamp, mesg[0], client_string);
        }
        pthread_mutex_unlock(&params_ptr->params_mutex);
        if (error_seen) {
            LOG_ERROR(
                "at %.3f, too few bytes (%d) in tcp message tag %d from %s\n",
                      timestamp, bytes, mesg[0], client_string);
        }
    }
    if (errno == ECONNRESET) {
        client_ptr->is_connected = false;
        LOG_ERROR("at %.3f, tcp client %s disconnected\n",
                  get_usecs() / (float)USECS_PER_SECOND, client_string);
    } else {
#define MAX_ERR_MSG 128
        char errmsg[MAX_ERR_MSG];
        LOG_ERROR("at %.3f, can't recv from tcp client %s, errno= %d; %s\n",
                  get_usecs() / (float)USECS_PER_SECOND, client_string,
                  errno, strerror_r(errno, errmsg, MAX_ERR_MSG));
    }
    return NULL;
}

void tcp_comms_send_string(Tcp_Comms* comms_ptr,
                           Text_Color color,
                           const char* string) {
    int ii;
    pthread_mutex_lock(&comms_ptr->connection_mutex);
    int connection_count = comms_ptr->connection_count;
    pthread_mutex_unlock(&comms_ptr->connection_mutex);
#define MAX_STRING 80
    char msg[MAX_STRING + 1];
    memset(msg, 0, MAX_STRING + 1);
    msg[0] = color;
    strncpy(&msg[1], string, MAX_STRING);
    for (ii = 0; ii < connection_count; ++ii) {
        if (comms_ptr->connection[ii].client.is_connected) {
            send(comms_ptr->connection[ii].client.fd, msg,
                 sizeof(msg), 0);
        }
    }
}

#define MAX_CONNECTIONS 256

/**
 * Start up the message_loop().
 */
static void* server_thread(void* void_args_ptr) {
    Tcp_Comms* comms_ptr = (Tcp_Comms*)void_args_ptr;
    int socket_fd = comms_ptr->server.fd;
    struct sockaddr_storage client_addr;

    socklen_t bytes = sizeof(client_addr);

    // We always keep around space for one unused client to work in.

    pthread_mutex_init(&comms_ptr->connection_mutex, NULL);
    comms_ptr->connection_count = 0;
    comms_ptr->connection = (Connection_Thread_Info*)
                   malloc(MAX_CONNECTIONS * sizeof(Connection_Thread_Info));
    Connection_Thread_Info* conn_args_ptr = &comms_ptr->connection[0];
    conn_args_ptr->client.saddr_len = sizeof(conn_args_ptr->client.saddr);
    while ((conn_args_ptr->client.fd =
                    accept(socket_fd,
                           (struct sockaddr*)&conn_args_ptr->client.saddr,
                           &conn_args_ptr->client.saddr_len)) >= 0) {
        pthread_t pthread_id;
        float timestamp = get_usecs() / (float)USECS_PER_SECOND;
        char client_string[MAX_CLIENT_STRING];
        conn_args_ptr->params_ptr = comms_ptr->params_ptr;
        conn_args_ptr->camera_ptr = comms_ptr->camera_ptr;
        conn_args_ptr->client.is_connected = true;
        get_ip_addr_str((const struct sockaddr*)&conn_args_ptr->client.saddr,
                        client_string, MAX_CLIENT_STRING);
        if (comms_ptr->connection_count >= MAX_CONNECTIONS) {
            LOG_STATUS("at %.3f, no space for tcp connection from %s\n",
                       timestamp, client_string);
            continue;
        }

        // Recv tcp_params from GUI

        Tcp_Params msg;
        int bytes = recv(conn_args_ptr->client.fd, &msg, sizeof(Tcp_Params), 0);
        if (bytes == sizeof(Tcp_Params)) {
            // GUI sent us tcp_params.  Use those values for selected fields.
            
            tcp_params_byte_swap(&msg);
            conn_args_ptr->params_ptr->cam_params = msg.cam_params;
            conn_args_ptr->params_ptr->test_img_enable = msg.test_img_enable;
            conn_args_ptr->params_ptr->yuv_write = msg.yuv_write;
            conn_args_ptr->params_ptr->jpg_write = msg.jpg_write;
            conn_args_ptr->params_ptr->detect_yuv = msg.detect_yuv;
            conn_args_ptr->params_ptr->blob_yuv_min[0] = msg.blob_yuv_min[0];
            conn_args_ptr->params_ptr->blob_yuv_min[1] = msg.blob_yuv_min[1];
            conn_args_ptr->params_ptr->blob_yuv_min[2] = msg.blob_yuv_min[2];
            conn_args_ptr->params_ptr->blob_yuv_max[0] = msg.blob_yuv_max[0];
            conn_args_ptr->params_ptr->blob_yuv_max[1] = msg.blob_yuv_max[1];
            conn_args_ptr->params_ptr->blob_yuv_max[2] = msg.blob_yuv_max[2];
            conn_args_ptr->params_ptr->analog_gain_target =
                                                       msg.analog_gain_target;
            conn_args_ptr->params_ptr->analog_gain_tol = msg.analog_gain_tol;
            conn_args_ptr->params_ptr->digital_gain_target =
                                                       msg.digital_gain_target;
            conn_args_ptr->params_ptr->digital_gain_tol = msg.digital_gain_tol;
            conn_args_ptr->params_ptr->crosshairs_x = msg.crosshairs_x;
            conn_args_ptr->params_ptr->crosshairs_y = msg.crosshairs_y;
        }

        // Send tcp_params to GUI
        
        msg = *conn_args_ptr->params_ptr;
        tcp_params_byte_swap(&msg);
        send(conn_args_ptr->client.fd, &msg, sizeof(Tcp_Params), 0);
        
        /* Log success, but first increment connection_count, so the new
           connection sees the log message. */

        pthread_mutex_lock(&comms_ptr->connection_mutex);
        ++comms_ptr->connection_count;
        pthread_mutex_unlock(&comms_ptr->connection_mutex);
        LOG_STATUS("at %.3f, new tcp connection from %s\n",
                   timestamp, client_string);

        // Start a new thread to handle all communications with the new client.

        int status = pthread_create(&pthread_id, NULL, connection_thread,
                                    conn_args_ptr);
        if (status != 0) {
            LOG_ERROR("at %.3f, tcp_comms can't pthread_create (%d)\n",
                      timestamp, status);
            return NULL;
        }

        // Make space for a new (unused) client.

        conn_args_ptr = &comms_ptr->connection[comms_ptr->connection_count];
        conn_args_ptr->client.saddr_len = sizeof(conn_args_ptr->client.saddr);
    }
    free(conn_args_ptr);
    char errmsg[MAX_ERR_MSG];
    LOG_ERROR("at %.3f, tcp_comms can't accept, errno=%d; %s\n",
              get_usecs() / (float)USECS_PER_SECOND, errno,
              strerror_r(errno, errmsg, MAX_ERR_MSG));
    return NULL;
}

int tcp_params_construct(Tcp_Params* params_ptr) {
    int status = pthread_mutex_init(&params_ptr->params_mutex, NULL);
    if (status != 0) return status;
    raspicamcontrol_set_defaults(&params_ptr->cam_params);
    params_ptr->test_img_enable = false;
    params_ptr->yuv_write = false;
    params_ptr->jpg_write = false;
    params_ptr->detect_yuv = false;
    params_ptr->blob_yuv_min[0] = 0;
    params_ptr->blob_yuv_min[1] = 0;
    params_ptr->blob_yuv_min[2] = 0;
    params_ptr->blob_yuv_max[0] = 0;
    params_ptr->blob_yuv_max[1] = 0;
    params_ptr->blob_yuv_max[2] = 0;
    params_ptr->analog_gain_target = -1.0;
    params_ptr->analog_gain_tol = 0.1;
    params_ptr->digital_gain_target = -1.0;
    params_ptr->digital_gain_tol = 0.1;
    params_ptr->crosshairs_x = 0;
    params_ptr->crosshairs_y = 0;
    return 0;
}

int tcp_comms_construct(Tcp_Comms* comms_ptr,
                        MMAL_COMPONENT_T* camera_ptr,
                        Tcp_Params* tcp_params_ptr,
                        unsigned short port_number) {
    char errmsg[MAX_ERR_MSG];
    comms_ptr->server.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (comms_ptr->server.fd < 0) {
        LOG_ERROR("tcp_comms can't create socket, errno=%d; %s\n",
                  errno, strerror_r(errno, errmsg, MAX_ERR_MSG));
        return -1;
    }
    const int true_flag = 1;
    if (setsockopt(comms_ptr->server.fd, SOL_SOCKET, SO_REUSEPORT,
                   &true_flag, sizeof(int)) < 0) {
        LOG_ERROR(
        "tcp_comms can't setsockopt(SO_REUSEPORT), errno=%d; %s; ignoring\n",
                  errno, strerror_r(errno, errmsg, MAX_ERR_MSG));
    }

    comms_ptr->server.saddr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in* saddr_ptr =
                                (struct sockaddr_in*)&comms_ptr->server.saddr;
    saddr_ptr->sin_family = AF_INET;
    saddr_ptr->sin_addr.s_addr = INADDR_ANY;
    saddr_ptr->sin_port = htons(port_number);

    if (bind(comms_ptr->server.fd,
             (struct sockaddr*)saddr_ptr, comms_ptr->server.saddr_len) < 0) {
        LOG_ERROR("tcp_comms can't bind, errno=%d; %s\n",
                  errno, strerror_r(errno, errmsg, MAX_ERR_MSG));
        return -1;
    }

    if (listen(comms_ptr->server.fd, 3) < 0) {
        LOG_ERROR("tcp_comms can't listen, errno=%d; %s\n",
                  errno, strerror_r(errno, errmsg, MAX_ERR_MSG));
        return -1;
    }

    // Initialize shared fields of *comms_ptr.

    comms_ptr->camera_ptr = camera_ptr;
    comms_ptr->params_ptr = tcp_params_ptr;

    // Start server_loop thread.

    pthread_t pthread_id;
    int status = pthread_create(&pthread_id, NULL, server_thread, comms_ptr);
    if (status != 0) {
        LOG_ERROR("tcp_comms can't pthread_create (%d)\n", status);
        return -1;
    }
    return 0;
}
