/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
 *******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#include "mmal/mmal.h"
#include "mmal/util/mmal_default_components.h"
#include "mmal/util/mmal_connection.h"
#include "mmal/util/mmal_util.h"
#include "overwrite_tif_tags.h"
#include "detect_color_blobs.h"
#include "yuv_color_space_image.h"
#include "udp_comms.h"
#include "tcp_comms.h"
#include "log_error.h"
#include "get_usecs.h"

#include "RaspiCamControl.c"

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2
// Port configuration for the splitter component
#define SPLITTER_CALLBACK_PORT 0
#define SPLITTER_PREVIEW_PORT 1

// Stills format information
#define STILLS_FRAME_RATE_NUM 3
#define STILLS_FRAME_RATE_DEN 1
/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define INPUT_PLUGIN_NAME "team 696 raspicam input plugin"

// Layer that preview window should be displayed on
#define PREVIEW_LAYER      2
// Frames rates of 0 implies variable, but denominator needs to be 1 to prevent div by 0
#define PREVIEW_FRAME_RATE_NUM 0
#define PREVIEW_FRAME_RATE_DEN 1

typedef struct {
    MMAL_COMPONENT_T* component_ptr;
    MMAL_POOL_T* pool_ptr;
} Component_Pool;

typedef struct {
    unsigned char* test_img;
    unsigned char test_img_y_value;
    Tcp_Params tcp_params;
    bool exposure_mode_is_frozen;
    unsigned char yuv_meas[3];
    unsigned int frame_no;
    int vwidth;
    int vheight;
    MMAL_POOL_T* pool_ptr;
    FILE* yuv_fp;
    Blob_List blob_list;
    pthread_mutex_t bbox_mutex;
#define MAX_BBOXES 20
    unsigned short bbox_element_count;
    unsigned short bbox_element[MAX_BBOXES * 4];
} Splitter_Callback_Data;

static init_splitter_callback_data(Splitter_Callback_Data* p) {
    int status;
    p->test_img = NULL;
    p->test_img_y_value = 128;
    if ((status = tcp_params_construct(&p->tcp_params)) != 0) {
        LOG_ERROR("can't pthread_mutex_init(params_mutex) (%d)\n", status);
    }
    p->exposure_mode_is_frozen = false;
    p->yuv_meas[0] = 128;
    p->yuv_meas[1] = 128;
    p->yuv_meas[2] = 128;
    p->frame_no = UINT_MAX; // encoder throws away 1st frame so we should too
    p->pool_ptr = NULL;
    p->yuv_fp = NULL;
#define MAX_RUNS 10000
#define MAX_BLOBS 1000
    p->blob_list = blob_list_init(MAX_RUNS, MAX_BLOBS);
    p->bbox_element_count = 0;
    pthread_mutex_init(&p->bbox_mutex, NULL);
}

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;
static pthread_mutex_t controls_mutex;
static int plugin_number;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

static int fps = 5;
static int width = -1;
static int height = -1;
static int vwidth = -1;
static int vheight = -1;
static int quality = 85;
static int usestills = 0;
static int wantPreview = 0;
static int wantTimestamp = 0;
static Splitter_Callback_Data splitter_callback_data;
Tcp_Comms tcp_comms;
static MMAL_PARAMETER_CAMERA_SETTINGS_T settings;
static Udp_Comms udp_comms;

static struct timeval timestamp;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct {
    /** semaphore which is posted when we reach end of frame
        (indicates end of capture or fault) */
    VCOS_SEMAPHORE_T complete_semaphore;
    MMAL_POOL_T *pool; /// pointer to our state in case required in callback
    Splitter_Callback_Data* splitter_data_ptr;
    uint32_t offset;
    unsigned int frame_no;
    unsigned int width;
    unsigned int height;
    unsigned int vwidth;
    unsigned int vheight;
} PORT_USERDATA;


//#define DSC
#ifdef DSC
FILE* debug_fp = NULL;
#endif

/*** plugin interface functions ***/

/**
 * Log contents of camera parameter structure.
 *
 * @param fps      Requested frames per second.
 * @param width    Camera image width (pixels).
 * @param height   Camera image height (pixels).
 * @param vwidth   Video output width (pixels).
 * @param vheight  Video output height (pixels).
 * @param params   Camera parameters.
 */
void raspicamcontrol_log_parameters(int fps,
                                    int width,
                                    int height,
                                    int vwidth,
                                    int vheight,
                                    const RASPICAM_CAMERA_PARAMETERS *params)
{
    const char *exp_mode = unmap_xref(params->exposureMode, exposure_map,
                                      exposure_map_size);
    const char *awb_mode = unmap_xref(params->awbMode, awb_map, awb_map_size);
    const char *image_effect = unmap_xref(params->imageEffect, imagefx_map,
                                          imagefx_map_size);
    const char *metering_mode = unmap_xref(params->exposureMeterMode,
                                           metering_mode_map,
                                           metering_mode_map_size);

    LOG_STATUS("FPS %d, Camera %d x %d, Video %d x %d\n",
               fps, width, height, vwidth, vheight);
    LOG_STATUS("Sharpness %d, Contrast %d, Brightness %d\n",
               params->sharpness, params->contrast, params->brightness);
    LOG_STATUS(
    "Saturation %d, ISO %d, Video Stabilisation %s, Exposure compensation %d\n",
               params->saturation, params->ISO,
               params->videoStabilisation ? "Yes": "No",
               params->exposureCompensation);
    LOG_STATUS("Exposure Mode '%s', AWB Mode '%s', Image Effect '%s'\n",
               exp_mode, awb_mode, image_effect);
    LOG_STATUS(
    "Metering Mode '%s', Colour Effect Enabled %s with U = %d, V = %d\n",
               metering_mode, params->colourEffects.enable ? "Yes":"No",
               params->colourEffects.u, params->colourEffects.v);
    LOG_STATUS("Rotation %d, hflip %s, vflip %s\n",
               params->rotation, params->hflip ? "Yes":"No",
               params->vflip ? "Yes":"No");
    LOG_STATUS("ROI x %lf, y %f, w %f h %f\n",
               params->roi.x, params->roi.y, params->roi.w, params->roi.h);
}


/******************************************************************************
  Description.: parse input parameters
  Input Value.: param contains the command line string and a pointer to globals
  Return Value: 0 if everything is ok
 ******************************************************************************/
int input_init(input_parameter *param, int plugin_no) {
    int status;
    Tcp_Params* tcp_params_ptr = &splitter_callback_data.tcp_params;
#ifdef DSC
    debug_fp = fopen("debug.txt", "w");
#endif
    int i;
    if ((status = pthread_mutex_init(&controls_mutex, NULL)) != 0) {
        LOG_ERROR("can't pthread_mutex_init(controls_mutex) (%d)\n", status);
        exit(EXIT_FAILURE);
    }

    if (init_splitter_callback_data(&splitter_callback_data) < 0) {
        exit(EXIT_FAILURE);
    }

    param->argv[0] = INPUT_PLUGIN_NAME;
    plugin_number = plugin_no;

    /* show all parameters for DBG purposes */
    for (i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},                       // 0
            {"help", no_argument, 0, 0},                    // 1
            {"x", required_argument, 0, 0},                 // 2
            {"width", required_argument, 0, 0},             // 3
            {"y", required_argument, 0, 0},                 // 4
            {"height", required_argument, 0, 0},            // 5
            {"fps", required_argument, 0, 0},               // 6
            {"framerate", required_argument, 0, 0},         // 7
            {"sh", required_argument, 0, 0},                // 8
            {"co", required_argument, 0, 0},                // 9
            {"br", required_argument, 0, 0},                // 10
            {"sa", required_argument, 0, 0},                // 11
            {"ISO", required_argument, 0, 0},               // 12
            {"vs", no_argument, 0, 0},                      // 13
            {"ev", required_argument, 0, 0},                // 14
            {"ex", required_argument, 0, 0},                // 15
            {"awb", required_argument, 0, 0},               // 16
            {"ifx", required_argument, 0, 0},               // 17
            {"cfx", required_argument, 0, 0},               // 18
            {"mm", required_argument, 0, 0},                // 19
            {"rot", required_argument, 0, 0},               // 20
            {"hf", no_argument, 0, 0},                      // 21
            {"vf", no_argument, 0, 0},                      // 22
            {"quality", required_argument, 0, 0},           // 23
            {"usestills", no_argument, 0, 0},               // 24
            {"preview", no_argument, 0, 0},                 // 25
            {"timestamp", no_argument, 0, 0},               // 26
            {"stats", no_argument, 0, 0},                   // 27
            {"drc", required_argument, 0, 0},               // 28
            {"shutter", required_argument, 0, 0},           // 29
            {"awbgainR", required_argument, 0, 0},          // 30
            {"awbgainB", required_argument, 0, 0},          // 31
            {"blobyuv", required_argument, 0, 0},           // 32
            {"writeyuv", no_argument, 0, 0},                // 33
            {"writejpg", no_argument, 0, 0},                // 34
            {"testimg", required_argument, 0, 0},           // 35
            {"vwidth", required_argument, 0, 0},            // 36
            {"vheight", required_argument, 0, 0},           // 37
            {"againtarget", required_argument, 0, 0},       // 38
            {"againtol", required_argument, 0, 0},          // 39
            {"dgaintarget", required_argument, 0, 0},       // 40
            {"dgaintol", required_argument, 0, 0},          // 41
            {0, 0, 0, 0}
        };

        c = getopt_long_only(param->argc, param->argv, "", long_options,
                             &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if (c == '?') {
            help();
            return 1;
        }

        switch(option_index) {
        /* h, help */
        case 0:
        case 1:
            help();
            return 1;
            break;
            /* width */
        case 2:
        case 3:
            width = atoi(optarg);
            width = VCOS_ALIGN_UP(width, 32); // DSC
            break;
            /* height */
        case 4:
        case 5:
            height = atoi(optarg);
            height = VCOS_ALIGN_UP(height, 16); // DSC
            break;
            /* fps */
        case 6:
        case 7:
            fps = atoi(optarg);
            break;
        case 8:
            //sharpness
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.sharpness);
            break;
        case 9:
            //contrast
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.contrast);
            break;
        case 10:
            //brightness
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.brightness);
            break;
        case 11:
            //saturation
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.saturation);
            break;
        case 12:
            //ISO
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.ISO);
            break;
        case 13:
            //video stabilisation
            tcp_params_ptr->cam_params.videoStabilisation = 1;
            break;
        case 14:
            //ev
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.exposureCompensation);
            break;
        case 15:
            //exposure
            tcp_params_ptr->cam_params.exposureMode = exposure_mode_from_string(optarg);
            break;
        case 16:
            //awb mode
            tcp_params_ptr->cam_params.awbMode = awb_mode_from_string(optarg);
            break;
        case 17:
            //img effect
            tcp_params_ptr->cam_params.imageEffect = imagefx_mode_from_string(optarg);
            break;
        case 18:
            //color effects
            sscanf(optarg, "%d:%d",
                   &tcp_params_ptr->cam_params.colourEffects.u,
                   &tcp_params_ptr->cam_params.colourEffects.u);
            tcp_params_ptr->cam_params.colourEffects.enable = 1;
            break;
        case 19:
            //metering mode
            tcp_params_ptr->cam_params.exposureMeterMode = metering_mode_from_string(optarg);
            break;
        case 20:
            //rotation
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.rotation);
            break;
        case 21:
            //hflip
            tcp_params_ptr->cam_params.hflip  = 1;
            break;
        case 22:
            //vflip
            tcp_params_ptr->cam_params.vflip = 1;
            break;
        case 23:
            //quality
            quality = atoi(optarg);
            break;
        case 24:
            //use stills
            usestills = 1;
            break;
        case 25:
            //display preview
            wantPreview = 1;
            break;
        case 26:
            //timestamp
            wantTimestamp = 1;
            break;
        case 27:
            // use stats
            tcp_params_ptr->cam_params.stats_pass = MMAL_TRUE;
            break;
        case 28:
            // Dynamic Range Compensation DRC
            tcp_params_ptr->cam_params.drc_level = drc_mode_from_string(optarg);
            break;
        case 29:
            // shutter speed in microseconds
            sscanf(optarg, "%d", &tcp_params_ptr->cam_params.shutter_speed);
            break;
        case 30:
            // awb gain red
            sscanf(optarg, "%f", &tcp_params_ptr->cam_params.awb_gains_r);
            break;
        case 31:
            // awb gain blue
            sscanf(optarg, "%f", &tcp_params_ptr->cam_params.awb_gains_b);
            break;
        case 32:
            // blobyuv
            {
                unsigned int opt0;
                unsigned int opt1;
                unsigned int opt2;
                unsigned int opt3;
                unsigned int opt4;
                unsigned int opt5;
                if (sscanf(optarg, "%u,%u,%u,%u,%u,%u",
                            &opt0, &opt1, &opt2, &opt3, &opt4, &opt5) != 6) {
                    LOG_ERROR("-blobyuv requires 6 unsigned int arguments\n");
                    help();
                    return 1;
                }
                if (opt0 > 255) opt0 = 255;
                if (opt1 > 255) opt1 = 255;
                if (opt2 > 255) opt2 = 255;
                if (opt3 > 255) opt3 = 255;
                if (opt4 > 255) opt4 = 255;
                if (opt5 > 255) opt5 = 255;
                tcp_params_ptr->blob_yuv_min[0] = opt0;
                tcp_params_ptr->blob_yuv_max[0] = opt1;
                tcp_params_ptr->blob_yuv_min[1] = opt2;
                tcp_params_ptr->blob_yuv_max[1] = opt3;
                tcp_params_ptr->blob_yuv_min[2] = opt4;
                tcp_params_ptr->blob_yuv_max[2] = opt5;
                tcp_params_ptr->detect_yuv = true;
            }
            break;
        case 33:
            //writeyuv
            tcp_params_ptr->yuv_write = true;
            break;
        case 34:
            //writejpg
            tcp_params_ptr->jpg_write = true;
            break;
        case 35:
            { //testimg
                int test_img_y_value;
                sscanf(optarg, "%d", &test_img_y_value);
                if (test_img_y_value > 255) test_img_y_value = 255;
                if (test_img_y_value < 0) test_img_y_value = 0;
                splitter_callback_data.test_img_y_value = test_img_y_value;
                break;
            }
            /* vwidth */
        case 36:
            vwidth = atoi(optarg);
            vwidth = VCOS_ALIGN_UP(vwidth, 32); // DSC
            break;
            /* height */
        case 37:
            vheight = atoi(optarg);
            vheight = VCOS_ALIGN_UP(vheight, 16); // DSC
            break;
        case 38:
            //againtarget
            sscanf(optarg, "%f", &tcp_params_ptr->analog_gain_target);
            break;
        case 39:
            //againtol
            sscanf(optarg, "%f", &tcp_params_ptr->analog_gain_tol);
            break;
        case 40:
            //dgaintarget
            sscanf(optarg, "%f", &tcp_params_ptr->digital_gain_target);
            break;
        case 41:
            //dgaintol
            sscanf(optarg, "%f", &tcp_params_ptr->digital_gain_tol);
            break;
        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    if (vwidth < 0 && width < 0) width = 640;
    if (vheight < 0 && height < 0) height = 480;
    if (vwidth < 0) vwidth = width;
    if (vheight < 0) vheight = height;
    if (width < 0) width = vwidth;
    if (height < 0) height = vheight;

    tcp_params_ptr->crosshairs_x = vwidth / 2;
    tcp_params_ptr->crosshairs_y = vheight / 2;
    splitter_callback_data.vwidth = vwidth;
    splitter_callback_data.vheight = vheight;

    pglobal = param->global;

    raspicamcontrol_log_parameters(fps, width, height, vwidth, vheight,
                                   &tcp_params_ptr->cam_params);
    return 0;
}

/******************************************************************************
  Description.: stops the execution of the worker thread
  Input Value.: -
  Return Value: 0
 ******************************************************************************/
int input_stop(int id) {
    pthread_cancel(worker);
    return 0;
}

/**************************************************
  Print which status
 **************************************************/
const char* log_mmal_status(MMAL_STATUS_T status) {
    if (status != MMAL_SUCCESS) {
        switch (status) {
        case MMAL_ENOMEM:
            return "Out of memory";
        case MMAL_ENOSPC:
            return "Out of resources (other than memory)";
        case MMAL_EINVAL:
            return "Argument is invalid";
        case MMAL_ENOSYS:
            return "Function not implemented";
        case MMAL_ENOENT:
            return "No such file or directory";
        case MMAL_ENXIO:
            return "No such device or address";
        case MMAL_EIO:
            return "I/O error";
        case MMAL_ESPIPE:
            return "Illegal seek";
        case MMAL_ECORRUPT:
            return "Data is corrupt";
        case MMAL_ENOTREADY:
            return "Component is not ready";
        case MMAL_ECONFIG:
            return "Component is not configured";
        case MMAL_EISCONN:
            return "Port is already connected";
        case MMAL_ENOTCONN:
            return "Port is disconnected";
        case MMAL_EAGAIN:
            return "Resource temporarily unavailable";
        case MMAL_EFAULT:
            return "Bad address";
        default:
            return "Unknown status error";
        }
    }
}


/**
 *  buffer header callback function for splitter
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void splitter_buffer_callback(MMAL_PORT_T *port,
                                     MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer;
    Splitter_Callback_Data *pData = (Splitter_Callback_Data*)port->userdata;
    /*
    fprintf(stderr, "splitter: %u %lld %lld %lld len= %d (%d x %d)\n",
            pData->frame_no, buffer->pts, buffer->dts, vcos_getmicrosecs64(),
            buffer->length, port->format->es->video.width,
            port->format->es->video.height);
    */

    if (pData != NULL) {
        unsigned char* img = buffer->data;
        mmal_buffer_header_mem_lock(buffer);
        pthread_mutex_lock(&pData->tcp_params.params_mutex);
        if (pData->tcp_params.test_img_enable) {
            img = pData->test_img;
        }
        if (pData->tcp_params.detect_yuv) {
            unsigned int cols = port->format->es->video.width;
            unsigned int rows = port->format->es->video.height;
            int64_t now = get_cam_host_usec(&udp_comms);
            Udp_Blob_List udp_blob_list;

            /* Capture the YUV color value at the crosshairs.
               (Crosshairs_x, crosshairs_y) is in video image coordinates.
               Convert to camera image coordinates. */

            int cam_x = pData->tcp_params.crosshairs_x * cols /
                        pData->vwidth;
            int cam_y = pData->tcp_params.crosshairs_y * rows /
                        pData->vheight;

            if (cam_x < 0 || cam_x >= cols ||
                cam_y < 0 || cam_y  >= rows) {
                cam_x = cols / 2;
                cam_y  = rows / 2;
            }
            yuv420_get_pixel(cols, rows, img, cam_x, cam_y, pData->yuv_meas);
            detect_color_blobs(&pData->blob_list,
                               pData->tcp_params.blob_yuv_min[0],
                               pData->tcp_params.blob_yuv_min[1],
                               pData->tcp_params.blob_yuv_max[1],
                               pData->tcp_params.blob_yuv_min[2],
                               pData->tcp_params.blob_yuv_max[2],
                               false, cols, rows, img);
#define MIN_PIXELS_PER_BLOB 30
            (void)blob_list_purge_small_bboxes(&pData->blob_list,
                                               MIN_PIXELS_PER_BLOB);
            pthread_mutex_lock(&pData->bbox_mutex);
            pData->bbox_element_count =
                copy_best_bounding_boxes(&pData->blob_list, MAX_BBOXES * 4,
                                         pData->bbox_element);
            pthread_mutex_unlock(&pData->bbox_mutex);
            //printf("bbox_element_count= %d\n", pData->bbox_element_count);
            udp_blob_list.msg_id = ID_UDP_BLOB_LIST;
            udp_blob_list.filler[0] = 0;
            udp_blob_list.filler[1] = 0;
            udp_blob_list.filler[2] = 0;
            udp_blob_list.blob_count = copy_best_bboxes_to_blob_stats_array(
                                                       &pData->blob_list,
                                                       MAX_UDP_BLOBS,
                                                       &udp_blob_list.blob[0]);
            (void)udp_comms_send_blobs_to_all(&udp_comms, now, &udp_blob_list);
        }
        pthread_mutex_unlock(&pData->tcp_params.params_mutex);
        mmal_buffer_header_mem_unlock(buffer);
        if (pData->yuv_fp != NULL) {
            //fwrite(buffer->data, 1, buffer->length, pData->yuv_fp);
            fwrite(img, 1, buffer->length, pData->yuv_fp);
        }
    } else {
        LOG_ERROR("Received a camera buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);
    ++pData->frame_no;

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status;
        new_buffer = mmal_queue_get(pData->pool_ptr->queue);
        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS) {
            LOG_ERROR("Unable to return a buffer to the splitter port");
        }
    }
}


/******************************************************************************
  Callback from mmal JPEG encoder
 ******************************************************************************/
static void encoder_buffer_callback(MMAL_PORT_T *port,
                                    MMAL_BUFFER_HEADER_T *buffer) {
    int complete = 0;

    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;
    /*
    fprintf(stderr, "encoder: %u %lld %lld %lld len= %d flags= %x\n",
            pData->frame_no, buffer->pts, buffer->dts, vcos_getmicrosecs64(),
            buffer->length, buffer->flags);
    */

    if (pData) {
        if (buffer->length) {
            mmal_buffer_header_mem_lock(buffer);

            //Write bytes
            /* copy JPG picture to global buffer */
            if (pData->offset == 0) {
                pthread_mutex_lock(&pglobal->in[plugin_number].db);
            }

#define SEND_BBOXES
#ifdef SEND_BBOXES
            if (buffer->data[0] == 0xff &&
                buffer->data[1] == 0xd8 &&
                buffer->data[2] == 0xff &&
                buffer->data[3] == 0xe1) {

                /* Send the bounding boxes and gains in the image packets by
                   overwriting the TIFF tags in the header supplied by
                   Broadcom. */

                const unsigned int SIZE_FIELD_OFFSET = 4;
                unsigned int total_header_bytes = SIZE_FIELD_OFFSET +
                    (((unsigned int)buffer->data[4] << 8) | buffer->data[5]);
                const unsigned int TIFF_TAGS_OFFSET = 12;
                int udp_comms_connections =
                            udp_comms_connection_count(&udp_comms);
                int64_t udp_comms_ping_age =
                            udp_comms_age_of_oldest_ping_response(&udp_comms);
                uint8_t flag0 =
                          pData->splitter_data_ptr->exposure_mode_is_frozen;
                uint8_t flag1 = (udp_comms_connections > 0);
                uint8_t flag2 = (udp_comms_ping_age > 2 * USECS_PER_SECOND);
                uint8_t flag3 =
                          pData->splitter_data_ptr->tcp_params.test_img_enable;
                uint8_t flags = flag3 << 3 | flag2 << 2 | flag1 << 1 | flag0;

                pthread_mutex_lock(&pData->splitter_data_ptr->bbox_mutex);
                overwrite_tif_tags(
                    pData->width, pData->height, pData->vwidth, pData->vheight,
                    pData->splitter_data_ptr->bbox_element_count,
                    pData->splitter_data_ptr->bbox_element, settings.exposure,
                    settings.analog_gain.num / (float)settings.analog_gain.den,
                    settings.digital_gain.num /
                                             (float)settings.digital_gain.den,
                    settings.awb_red_gain.num /
                                             (float)settings.awb_red_gain.den,
                    settings.awb_blue_gain.num /
                                             (float)settings.awb_blue_gain.den,
                    pData->splitter_data_ptr->yuv_meas,
                    flags, buffer->data);
                pthread_mutex_unlock(&pData->splitter_data_ptr->bbox_mutex);
            }
#endif
#ifdef DSC
            if (debug_fp != NULL) {
                // Hex dump of jpeg image buffer.
                if (buffer->data[0] == 0xff &&
                        buffer->data[1] == 0xd8 &&
                        buffer->data[2] == 0xff &&
                        buffer->data[3] == 0xe1) {
                    unsigned int len =
                        ((unsigned int)buffer->data[4] << 8) | buffer->data[5];
                    fprintf(debug_fp, "ff d8 ff e1");
                    int ii;
                    for (ii = 0; ii < len; ++ii) {
                        fprintf(debug_fp, " %02x", buffer->data[ii + 4]);
                    }
                    fprintf(debug_fp, "\n");
                    fflush(debug_fp);
                }
            }
#endif
            memcpy(pData->offset + pglobal->in[plugin_number].buf,
                   buffer->data, buffer->length);
            pData->offset += buffer->length;
            mmal_buffer_header_mem_unlock(buffer);
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END |
                             MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)) {
            //set frame size
            pglobal->in[plugin_number].size = pData->offset;

            //Set frame timestamp
            if(wantTimestamp) {
                gettimeofday(&timestamp, NULL);
                pglobal->in[plugin_number].timestamp = timestamp;
            }

            //mark frame complete
            complete = 1;

            pData->offset = 0;
            ++pData->frame_no;
            /* signal fresh_frame */
            pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
            pthread_mutex_unlock(&pglobal->in[plugin_number].db);
        }
    } else {
        LOG_ERROR("Received a encoder buffer callback with no state\n");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        MMAL_STATUS_T status;
        MMAL_BUFFER_HEADER_T *new_buffer;

        new_buffer = mmal_queue_get(pData->pool->queue);

        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);

            if (status != MMAL_SUCCESS) {
                LOG_ERROR(
                       "Failed returning a buffer to the encoder port (%s)\n",
                          log_mmal_status(status));
            }
        } else {
            LOG_ERROR("Unable to return a buffer to the encoder port\n");
        }
    }
    if (complete) vcos_semaphore_post(&(pData->complete_semaphore));
}

typedef struct {
    MMAL_COMPONENT_T* camera_ptr;
    Splitter_Callback_Data* splitter_data_ptr;
} Camera_Control_Callback_Data;

static void camera_control_callback_data_construct(
                                 MMAL_COMPONENT_T* camera_ptr,
                                 Splitter_Callback_Data* splitter_data_ptr,
                                 Camera_Control_Callback_Data* data_ptr) {
    data_ptr->camera_ptr = camera_ptr;
    data_ptr->splitter_data_ptr = splitter_data_ptr;
}


/**
 * buffer header callback function for camera control
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback(MMAL_PORT_T *port,
                                    MMAL_BUFFER_HEADER_T *buffer) {
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
#define DSC1
#ifdef DSC1
        Camera_Control_Callback_Data *data_ptr =
                                (Camera_Control_Callback_Data*)port->userdata;
        MMAL_EVENT_PARAMETER_CHANGED_T *param =
                                (MMAL_EVENT_PARAMETER_CHANGED_T*)buffer->data;
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS: {
                Splitter_Callback_Data* splitter_data_ptr =
                                                data_ptr->splitter_data_ptr;
                Tcp_Params* tcp_params_ptr = &splitter_data_ptr->tcp_params;
                MMAL_PARAMETER_CAMERA_SETTINGS_T* sptr = &settings;
                *sptr = *(MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
                pthread_mutex_lock(&tcp_params_ptr->params_mutex);

                // If auto freeze exposure mode is turned on...

                if ((tcp_params_ptr->analog_gain_target > 0.0 ||
                     tcp_params_ptr->digital_gain_target > 0.0) &&
                    tcp_params_ptr->cam_params.exposureMode !=
                                              MMAL_PARAM_EXPOSUREMODE_OFF) {
                    // Measure current analog and digital gains.

                    float analog_gain = settings.analog_gain.num /
                                        (float)settings.analog_gain.den;
                    float analog_gain_error = fabs(analog_gain -
                                         tcp_params_ptr->analog_gain_target);
                    float digital_gain = settings.digital_gain.num /
                                        (float)settings.digital_gain.den;
                    float digital_gain_error = fabs(digital_gain -
                                         tcp_params_ptr->digital_gain_target);
                    bool analog_gain_is_wrong =
                        tcp_params_ptr->analog_gain_target > 0.0 &&
                        analog_gain_error > tcp_params_ptr->analog_gain_tol;
                    bool digital_gain_is_wrong =
                        tcp_params_ptr->digital_gain_target > 0.0 &&
                        digital_gain_error > tcp_params_ptr->digital_gain_tol;
                    if (analog_gain_is_wrong || digital_gain_is_wrong) {
                        // One or both gain measurements are incorrect.

                        if (splitter_data_ptr->exposure_mode_is_frozen) {
                            // Unfreeze exposure mode to allow gains to adjust.

                            LOG_ERROR("unfreeze exposure mode\n");
                            raspicamcontrol_set_exposure_mode(
                                data_ptr->camera_ptr,
                                tcp_params_ptr->cam_params.exposureMode);
                            splitter_data_ptr->exposure_mode_is_frozen = false;
                        }
                    } else {
                        // Analog and digital gain measurements are correct.

                        if (!splitter_data_ptr->exposure_mode_is_frozen) {
                            // Freeze the exposure mode.

                            LOG_ERROR("freeze exposure mode\n");
                            raspicamcontrol_set_exposure_mode(
                                              data_ptr->camera_ptr,
                                              MMAL_PARAM_EXPOSUREMODE_OFF);
                            splitter_data_ptr->exposure_mode_is_frozen = true;
                        }
                    }
                } else {
                    if (splitter_data_ptr->exposure_mode_is_frozen) {
                        // Unfreeze exposure mode to allow gains to adjust.

                        LOG_ERROR("unfreeze exposure mode\n");
                        raspicamcontrol_set_exposure_mode(
                            data_ptr->camera_ptr,
                            tcp_params_ptr->cam_params.exposureMode);
                        splitter_data_ptr->exposure_mode_is_frozen = false;
                    }
                }
                pthread_mutex_unlock(&tcp_params_ptr->params_mutex);
                     
                 /*
                    printf(
                    "Exposure now %u, analog gain %u/%u, digital gain %u/%u ",
                    settings.exposure,
                    settings.analog_gain.num, settings.analog_gain.den,
                    settings.digital_gain.num, settings.digital_gain.den);
                    printf("AWB R=%u/%u, B=%u/%u\n",
                    settings.awb_red_gain.num, settings.awb_red_gain.den,
                    settings.awb_blue_gain.num,
                    settings.awb_blue_gain.den);
                  */
                 break;
             }
        }
#endif
    } else {
        LOG_ERROR("Received unexpected camera control callback event %d",
                buffer->cmd);
    }
    mmal_buffer_header_release(buffer);
}

/**
 * Create the resizer component, set up its ports
 *
 * @param camera_component Pointer to camera component
 *
 * @return The newly created resizer component and pool.  On failure
 *         Component_Pool.component_ptr will be NULL, and the error will be
 *         logged.
 *
 */
static Component_Pool create_resizer_component(
                                        MMAL_COMPONENT_T* camera_component,
                                        int width,
                                        int height) {
    Component_Pool pair = { NULL, NULL };
    MMAL_COMPONENT_T *resizer = 0;
    MMAL_PORT_T *resizer_output = NULL;
    MMAL_ES_FORMAT_T *format;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;
    int i;

    if (camera_component == NULL) {
        status = MMAL_ENOSYS;
        LOG_ERROR("Camera component must be created before resizer (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    /* Create the component */


#define MMAL_COMPONENT_RESIZER "vc.ril.resize"
    //#define MMAL_COMPONENT_RESIZER "vc.ril.isp"
    status = mmal_component_create(MMAL_COMPONENT_RESIZER, &resizer);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("Failed to create resizer component (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    if (resizer->input_num == 0) {
        status = MMAL_ENOSYS;
        LOG_ERROR("resizer has no input port (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    if (resizer->output_num == 0) {
        status = MMAL_ENOSYS;
        LOG_ERROR("resizer has no output port (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames: */
    mmal_format_copy(resizer->input[0]->format,
            camera_component->output[MMAL_CAMERA_VIDEO_PORT]->format);


    if (resizer->input[0]->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        resizer->input[0]->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }
    status = mmal_port_format_commit(resizer->input[0]);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("Unable to set format on resizer input port (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    /* Resizer can do format conversions, configure format for its output
       port: */
    mmal_format_copy(resizer->output[0]->format, resizer->input[0]->format);
    if (resizer->output[0]->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        resizer->output[0]->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    /* Here is where we change the size. */

    width = VCOS_ALIGN_UP(width, 32);
    height = VCOS_ALIGN_UP(height, 16);
    resizer->output[0]->format->es->video.width = width;
    resizer->output[0]->format->es->video.height = height;
    resizer->output[0]->format->es->video.crop.width = width;
    resizer->output[0]->format->es->video.crop.height = height;
    status = mmal_port_format_commit(resizer->output[0]);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("Unable to set format on resizer output port %d (%s)\n",
                i, log_mmal_status(status));
        goto error;
    }

    /* Enable component */

    status = mmal_component_enable(resizer);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("resizer component couldn't be enabled (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */

    resizer_output = resizer->output[0];
    pool = mmal_port_pool_create(resizer_output, resizer_output->buffer_num,
            resizer_output->buffer_size);

    if (!pool) {
        LOG_ERROR(
        "Failed to create buffer header pool for resizer output port %s",
                  resizer_output->name);
    }

    pair.pool_ptr = pool;
    pair.component_ptr = resizer;
    return pair;
error:
    if (resizer) mmal_component_destroy(resizer);
    return pair;
}



/**
 * Create the splitter component, set up its ports
 *
 * @param camera_component Pointer to camera component
 *
 * @return The newly created splitter component and pool.  On failure
 *         Component_Pool.component_ptr will be NULL, and the error will be
 *         logged.
 *
 */
static Component_Pool create_splitter_component(
                                          MMAL_COMPONENT_T* camera_component) {
    Component_Pool pair = { NULL, NULL };
    MMAL_COMPONENT_T *splitter = 0;
    MMAL_PORT_T *splitter_output = NULL;
    MMAL_ES_FORMAT_T *format;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;
    int i;

    if (camera_component == NULL) {
        status = MMAL_ENOSYS;
        LOG_ERROR("Camera component must be created before splitter (%s)\n",
                log_mmal_status(status));
        goto error;
    }

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER,
                                   &splitter);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("Failed to create splitter component (%s)\n",
                  log_mmal_status(status));
        goto error;
    }

    if (!splitter->input_num) {
        status = MMAL_ENOSYS;
        LOG_ERROR("Splitter doesn't have any input port (%s)\n",
                  log_mmal_status(status));
        goto error;
    }

    if (splitter->output_num < 2) {
        status = MMAL_ENOSYS;
        LOG_ERROR("Splitter doesn't have enough output ports (%s)\n",
                  log_mmal_status(status));
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames: */
    mmal_format_copy(splitter->input[0]->format,
                camera_component->output[MMAL_CAMERA_PREVIEW_PORT]->format);

    if (splitter->input[0]->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        splitter->input[0]->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    status = mmal_port_format_commit(splitter->input[0]);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("Unable to set format on splitter input port (%s)\n",
                  log_mmal_status(status));
        goto error;
    }

    /* Splitter can do format conversions, configure format for its output
       port. */

    for (i = 0; i < splitter->output_num; i++) {
        mmal_format_copy(splitter->output[i]->format,
                         splitter->input[0]->format);

        if (i == SPLITTER_CALLBACK_PORT) {
            format = splitter->output[i]->format;
            format->encoding = MMAL_ENCODING_I420;
            format->encoding_variant = MMAL_ENCODING_I420;
        }

        status = mmal_port_format_commit(splitter->output[i]);

        if (status != MMAL_SUCCESS) {
            LOG_ERROR("Unable to set format on splitter output port %d (%s)\n",
                      i, log_mmal_status(status));
            goto error;
        }
    }

    /* Enable component */
    status = mmal_component_enable(splitter);

    if (status != MMAL_SUCCESS) {
        LOG_ERROR("splitter component couldn't be enabled (%s)\n",
                  log_mmal_status(status));
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    splitter_output = splitter->output[SPLITTER_CALLBACK_PORT];
    pool = mmal_port_pool_create(splitter_output, splitter_output->buffer_num,
                                 splitter_output->buffer_size);

    if (!pool) {
        LOG_ERROR(
            "Failed to create buffer header pool for splitter output port %s",
                  splitter_output->name);
    }

    pair.pool_ptr = pool;
    pair.component_ptr = splitter;
    return pair;
error:
    if (splitter) mmal_component_destroy(splitter);
    return pair;
}


/**
 * Destroy the splitter component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_splitter_component(Component_Pool* pair_ptr) {
    // Get rid of any port buffers first

    if (pair_ptr->pool_ptr) {
        mmal_port_pool_destroy(
                pair_ptr->component_ptr->output[SPLITTER_CALLBACK_PORT],
                pair_ptr->pool_ptr);
        pair_ptr->pool_ptr = NULL;
    }

    if (pair_ptr->component_ptr) {
        mmal_component_destroy(pair_ptr->component_ptr);
        pair_ptr->component_ptr = NULL;
    }
}


/******************************************************************************
  Description.: starts the worker thread and allocates memory
  Input Value.: -
  Return Value: 0
 ******************************************************************************/
int input_run(int id) {
    pglobal->in[id].buf = malloc(width * height * 3);
    if (pglobal->in[id].buf == NULL) {
        LOG_ERROR("can't malloc(%d)\n", width * height * 3);
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&worker, 0, worker_thread, NULL) != 0) {
        free(pglobal->in[id].buf);
        LOG_ERROR("can't pthread_create(worker_thread)\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(worker);
    return 0;
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port,
                                   MMAL_PORT_T *input_port,
                                   MMAL_CONNECTION_T **connection) {
    MMAL_STATUS_T status;
    status = mmal_connection_create(connection, output_port, input_port,
                                    MMAL_CONNECTION_FLAG_TUNNELLING |
                                    MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
    if (status == MMAL_SUCCESS) {
        status = mmal_connection_enable(*connection);
        if (status != MMAL_SUCCESS) {
            LOG_ERROR("can't mmal_connection_enable (%s)\n",
                      log_mmal_status(status));
            mmal_connection_destroy(*connection);
            *connection = NULL;
        }
    } else {
        LOG_ERROR("can't mmal_connection_create (%s)\n",
                  log_mmal_status(status));
        *connection = NULL;
    }
    return status;
}


/******************************************************************************
  Description.: print help message
  Input Value.: -
  Return Value: -
 ******************************************************************************/
void help(void)
{
  fprintf(stderr,
" ---------------------------------------------------------------\n" \
" Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
" ---------------------------------------------------------------\n" \
" The following parameters can be passed to this plugin:\n\n" \
" [-fps | --framerate]...: set video framerate, default 5 frame/sec \n"\
" [-x | --width ]........: width of frame capture, default 640\n" \
" [-y | --height]........: height of frame capture, default 480 \n"\
" [--vwidth ]............: width of video stream\n" \
" [--vheight]............: height of video stream\n"\
" [-quality].............: set JPEG quality 0-100, default 85 \n"\
" [-usestills]...........: uses stills mode instead of video mode \n"\
" [-preview].............: Enable full screen preview\n"\
" [-timestamp]...........: Get timestamp for each frame\n"
" [-writeyuv]............: Write to out_WWWW_HHHH.yuv\n"
" \n"\
" -sh  : Set image sharpness (-100 to 100)\n"\
" -co  : Set image contrast (-100 to 100)\n"\
" -br  : Set image brightness (0 to 100)\n"\
" -sa  : Set image saturation (-100 to 100)\n"\
" -ISO : Set capture ISO\n"\
" -vs  : Turn on video stablisation\n"\
" -ev  : Set EV compensation\n"\
" -ex  : Set exposure mode (see raspistill notes)\n"\
" -awb : Set AWB mode (see raspistill notes)\n"\
" -ifx : Set image effect (see raspistill notes)\n"\
" -cfx : Set colour effect (U:V)\n"\
" -mm  : Set metering mode (see raspistill notes)\n"\
" -rot : Set image rotation (0-359)\n"\
" -stats : Compute image stats for each picture (reduces noise for -usestills)\n"\
" -drc : Dynamic range compensation level (see raspistill notes)\n"\
" -hf  : Set horizontal flip\n"\
" -vf  : Set vertical flip\n"\
" ---------------------------------------------------------------\n");

}

/******************************************************************************
  Description.: setup mmal and callback
  Input Value.: arg is not used
  Return Value: NULL
 ******************************************************************************/
void *worker_thread(void *arg) {
    int i = 0;
    MMAL_COMPONENT_T *camera = 0;

    // Set cleanup handler to cleanup allocated resources.

    pthread_cleanup_push(worker_cleanup, NULL);

    // Dont' let this thread be cancelled.  It needs to clean up mmal on exit.

    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) != 0) {
        LOG_ERROR("can't pthread_setcancelstate\n");
        exit(EXIT_FAILURE);
    }

    usecs_init();

    /* Start udp_comms thread. */

//#define DEFAULT_UDP_COMMS_CLIENT_NAME "127.0.0.1"
#define DEFAULT_UDP_COMMS_CLIENT_NAME NULL
#define DEFAULT_UDP_COMMS_CLIENT_PORT 10696
#define MAX_ROUND_TRIP_USECS (USECS_PER_SECOND / 8)
#define CLOCK_SAMPLES 500
    udp_comms_construct(&udp_comms, DEFAULT_UDP_COMMS_CLIENT_NAME, 
            DEFAULT_UDP_COMMS_CLIENT_PORT, CLOCK_SAMPLES,
            MAX_ROUND_TRIP_USECS, false);

    //Camera variables

    MMAL_COMPONENT_T *preview = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_STATUS_T status;
    MMAL_PORT_T *camera_preview_port = NULL;
    MMAL_PORT_T *camera_video_port = NULL;
    MMAL_PORT_T *camera_still_port = NULL;
    MMAL_PORT_T *preview_input_port = NULL;
    MMAL_PORT_T *splitter_callback_port = NULL;

    // Preview variables

    MMAL_CONNECTION_T *preview_connection = 0;

    //Encoder variables
    
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL;
    MMAL_PORT_T *encoder_output = NULL;
    MMAL_POOL_T *pool;
    MMAL_CONNECTION_T *encoder_connection;

    // Splitter variables

    Component_Pool splitter_pair;
    MMAL_CONNECTION_T *splitter_connection = NULL;

    // Resizer variables

    Component_Pool resizer_pair;
    MMAL_CONNECTION_T *resizer_connection = NULL;

    // FPS count

    struct timespec t_start, t_finish;
    double t_elapsed;
    int frames;

    // Create camera

    bcm_host_init();
    DBG("Host init, starting mmal stuff\n");

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't mmal_component_create(camera)\n");
        exit(EXIT_FAILURE);
    }

    if (!camera->output_num) {
        LOG_ERROR("camera has no output port\n");
        mmal_component_destroy(camera);
        exit(EXIT_FAILURE);
    }


#define DEFAULT_TCP_COMMS_PORT 10696
    if (tcp_comms_construct(&tcp_comms, camera,
                            &splitter_callback_data.tcp_params,
                            DEFAULT_TCP_COMMS_PORT)) {
        exit(EXIT_FAILURE);
    }

    camera_preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    camera_video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    camera_still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable camera control port
    // Enable the camera, and tell it its control callback function
#ifdef DSC1
    Camera_Control_Callback_Data camera_control_callback_data;
    camera_control_callback_data_construct(
                                 camera,
                                 &splitter_callback_data,
                                 &camera_control_callback_data);
    camera->control->userdata =
               (struct MMAL_PORT_USERDATA_T *)&camera_control_callback_data;

    MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
                    { { MMAL_PARAMETER_CHANGE_EVENT_REQUEST,
                          sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T) },
                      MMAL_PARAMETER_CAMERA_SETTINGS, 1 };
    (void)mmal_port_parameter_set(camera->control, &change_event_request.hdr);

    /* Doesn't work with video
       MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request2 =
       { { MMAL_PARAMETER_CHANGE_EVENT_REQUEST,
           sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T) },
         MMAL_PARAMETER_CAPTURE_STATUS, 1 };
       (void)mmal_port_parameter_set(camera->control,
                                     &change_event_request2.hdr); */
#endif
    status = mmal_port_enable(camera->control, camera_control_callback);
#ifdef DSC2
    /* THIS IS HOW YOU GET THE TIME FROM THE CAMERA. 
       Time is in microseconds (since boot).
       This should match the frame time stamp (if you are in raw mode). */

    typedef struct {
        MMAL_PARAMETER_HEADER_T hdr;
        uint64_t time;
    } Time_Request;
    Time_Request time_request = { { MMAL_PARAMETER_SYSTEM_TIME,
                                    sizeof(Time_Request) },
                                  0 };
    mmal_port_parameter_get(camera->control, &time_request.hdr);
    struct timespec sys_time;
    clock_gettime(CLOCK_MONOTONIC, &sys_time);
    printf("*********** cam_time = %lld %lld\n", time_request.time,
           (sys_time.tv_sec * (int64_t)1000000 +
            sys_time.tv_nsec / (int64_t)1000));

    /* Query GPU for how much memory it is using. See raspicamcontrol_get_mem().
       Memory is in megabytes.  The split between GPU and CPU memory is set
       by running "sudo raspi-config" */

    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0) {
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    }
    printf("*********** gpu_mem = %d\n", gpu_mem);
#endif

    if (status) {
        LOG_ERROR("can't mmal_port_enable(camera_control_callback)\n");
        mmal_component_destroy(camera);
        exit(EXIT_FAILURE);
    }
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof (cam_config)},
            .max_stills_w = width,
            .max_stills_h = height,
            .stills_yuv422 = 0,
            .one_shot_stills = (usestills ? 1 : 0),
            .max_preview_video_w = width,
            .max_preview_video_h = height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
            // consider .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
        };
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }


    //Set camera parameters
    if (raspicamcontrol_set_all_parameters(
                      camera, &splitter_callback_data.tcp_params.cam_params)) {
        LOG_ERROR("can't raspicamcontrol_set_all_parameters\n");
        exit(EXIT_FAILURE);
    }

    // Set the encode format on the Preview port

    format = camera_preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;  // DSC
    format->es->video.width = VCOS_ALIGN_UP(width, 32);
    format->es->video.height = VCOS_ALIGN_UP(height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = width;
    format->es->video.crop.height = height;
    format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

    status = mmal_port_format_commit(camera_preview_port);

    // Create preview component

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                   &preview);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't mmal_component_create(preview)\n");
        exit(EXIT_FAILURE);
    }
    if (!preview->input_num) {
        status = MMAL_ENOSYS;
        LOG_ERROR("preview component has no input ports\n");
        exit(EXIT_FAILURE);
    }
    preview_input_port = preview->input[0];

    MMAL_DISPLAYREGION_T param;
    param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    param.set = MMAL_DISPLAY_SET_LAYER;
    param.layer = PREVIEW_LAYER;
    param.set |= MMAL_DISPLAY_SET_ALPHA;
    param.alpha = 255;
    param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
    param.fullscreen = 1;
    status = mmal_port_parameter_set(preview_input_port, &param.hdr);

    if (status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
        LOG_ERROR("can't mmal_port_parameter_set(preview_input_port) (%u)\n",
                status);
        exit(EXIT_FAILURE);
    }

    if (!usestills) {
        // Set the encode format on the video port
        format = camera_video_port->format;
        format->encoding_variant = MMAL_ENCODING_I420;
        format->encoding = MMAL_ENCODING_I420;
        //format->encoding_variant = MMAL_ENCODING_OPAQUE; //DSC
        format->es->video.width = width;
        format->es->video.height = height;
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = width;
        format->es->video.crop.height = height;
        format->es->video.frame_rate.num = fps;
        format->es->video.frame_rate.den = 1;
        status = mmal_port_format_commit(camera_video_port);
        if (status) {
            LOG_ERROR("can't mmal_port_format_commit(camera_video_port)\n");
            exit(EXIT_FAILURE);
        }

        // Ensure there are enough buffers to avoid dropping frames
        if (camera_video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
            camera_video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
        }
    }

    // Set our stills format on the stills (for encoder) port

    format = camera_still_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = width;
    format->es->video.height = height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = width;
    format->es->video.crop.height = height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

    status = mmal_port_format_commit(camera_still_port);
    if (status) {
        LOG_ERROR("can't mmal_port_format_commit(camera_still_port)\n");
        mmal_component_destroy(camera);
        exit(EXIT_FAILURE);
    }

    /* Ensure there are enough buffers to avoid dropping frames */

    if (camera_still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        camera_still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    /* Enable camera component */

    status = mmal_component_enable(camera);
    if (status) {
        LOG_ERROR("can't mmal_component_enable(camera) (%u)\n", status);
        mmal_component_destroy(camera);
        exit(EXIT_FAILURE);
    }

    /* Enable preview component */

    status = mmal_component_enable(preview);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't mmal_component_enable(preview) (%u)\n", status);
        mmal_component_destroy(preview);
        mmal_component_destroy(camera);
        exit(EXIT_FAILURE);
    }

    // Create Encoder

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER,
                                   &encoder);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't mmal_component_create(encoder) (%u)\n", status);
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }
    if (!encoder->input_num || !encoder->output_num) {
        LOG_ERROR("not enough encoder ports\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }
    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want the same format on input and output.

    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Specify out output format JPEG.

    encoder_output->format->encoding = MMAL_ENCODING_JPEG;
    encoder_output->buffer_size = encoder_output->buffer_size_recommended;
    if (encoder_output->buffer_size < encoder_output->buffer_size_min) {
        encoder_output->buffer_size = encoder_output->buffer_size_min;
    }
    encoder_output->buffer_num = encoder_output->buffer_num_recommended;
    if (encoder_output->buffer_num < encoder_output->buffer_num_min) {
        encoder_output->buffer_num = encoder_output->buffer_num_min;
    }

    // Commit the port changes to the output port

    status = mmal_port_format_commit(encoder_output);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't mmal_port_format_commit(encoder_output)\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }

    // Set the JPEG quality level

    status = mmal_port_parameter_set_uint32(encoder_output,
             MMAL_PARAMETER_JPEG_Q_FACTOR,
             quality);
    if (status != MMAL_SUCCESS) {
        LOG_ERROR("can't set jpeg quality\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }

    // Enable encoder component

    status = mmal_component_enable(encoder);
    if (status) {
        LOG_ERROR("can't enable encoder\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }
    DBG("Encoder enabled, creating pool and connecting ports\n");

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num,
                                 encoder_output->buffer_size);
    if (!pool) {
        LOG_ERROR("can't mmal_port_pool_create(encoder_output)\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }

    // Set up splitter component

    status = MMAL_SUCCESS;
    splitter_pair = create_splitter_component(camera);
    if (splitter_pair.component_ptr == NULL) status = MMAL_ENOSYS;

    if (status == MMAL_SUCCESS) {
        if (wantPreview) {
            // Connect camera preview output to splitter input

            status = connect_ports(camera_preview_port,
                                   splitter_pair.component_ptr->input[0],
                                   &splitter_connection);

            if (status != MMAL_SUCCESS) {
                LOG_ERROR(
                "can't connect_ports(camera_preview, splitter_input) (%s)\n",
                          log_mmal_status(status));
            } else {
                // Connect splitter preview output to preview input

                status = connect_ports(
                 splitter_pair.component_ptr->output[SPLITTER_PREVIEW_PORT],
                                       preview_input_port,
                                       &preview_connection);
                if (status != MMAL_SUCCESS) {
                    LOG_ERROR(
                "can't connect_ports(splitter_preview, preview_input) (%s)\n",
                            log_mmal_status(status));
                }
            }
        } else {
            // Connect camera to splitter

            status = connect_ports(camera_preview_port,
                                   splitter_pair.component_ptr->input[0],
                                   &splitter_connection);

            if (status != MMAL_SUCCESS) {
                LOG_ERROR(
                  "can't connect_ports(camera_preview, splitter_input) (%s)\n",
                          log_mmal_status(status));
            }
        }
    } 

    resizer_pair = create_resizer_component(camera, vwidth, vheight);

    // Now connect the camera to the encoder

    if (status == MMAL_SUCCESS) {
        if (usestills){
            status = connect_ports(camera_still_port, encoder->input[0],
                                   &encoder_connection);
        } else {
            status = connect_ports(camera_video_port,
                                   resizer_pair.component_ptr->input[0],
                                   &resizer_connection);
            status = connect_ports(resizer_pair.component_ptr->output[0],
                                   encoder->input[0], &encoder_connection);
        }
    }

    if (status == MMAL_SUCCESS && splitter_connection != NULL) {
        splitter_callback_data.pool_ptr = splitter_pair.pool_ptr;
        splitter_callback_port =
                   splitter_pair.component_ptr->output[SPLITTER_CALLBACK_PORT];
        splitter_callback_port->userdata =
                   (struct MMAL_PORT_USERDATA_T *)&splitter_callback_data;
        if (splitter_callback_data.tcp_params.yuv_write) {
            char filename[32];
            snprintf(filename, 32, "out.yuv");
            splitter_callback_data.yuv_fp = fopen(filename, "wb");
            fprintf(splitter_callback_data.yuv_fp,
                    "#!YUV420 %7u,%7u\n", width, height);
        }

        /* Create the YUV test image. */

        splitter_callback_data.test_img = (unsigned char*)malloc(
                                                       width * height * 3 / 2);
        yuv_color_space_image(width, height,
                              splitter_callback_data.test_img_y_value,
                              splitter_callback_data.test_img);

        status = mmal_port_enable(splitter_callback_port,
                                  splitter_buffer_callback);
        if (status != MMAL_SUCCESS) {
            LOG_ERROR("can't mmal_port_enable(splitter_buffer_callback)\n");
        }
    }

    if (status) {
        if (preview_connection) {
            mmal_connection_destroy(preview_connection);
        }
        if (splitter_connection) {
            mmal_connection_destroy(splitter_connection);
        }
        if (encoder_connection) {
            mmal_connection_destroy(encoder_connection);
        }
        if (resizer_connection) {
            mmal_connection_destroy(resizer_connection);
        }
        if (preview) {
            mmal_component_destroy(preview);
        }
        mmal_component_destroy(camera);
        if (encoder) {
            mmal_component_destroy(encoder);
        }
        exit(EXIT_FAILURE);
    }

    /* Set up our userdata - this is passed though to the callback where we
       need the information. */

    PORT_USERDATA callback_data;
    callback_data.pool = pool;
    callback_data.offset = 0;
    callback_data.frame_no = 0;
    callback_data.width = width;   // width of original image
    callback_data.height = height; // height of original image
    callback_data.vwidth = vwidth;   // width of encode image
    callback_data.vheight = vheight; // height of encode image
    callback_data.splitter_data_ptr = &splitter_callback_data;

    vcos_assert(vcos_semaphore_create(&callback_data.complete_semaphore,
                                      "RaspiStill-sem", 0) == VCOS_SUCCESS);

    encoder->output[0]->userdata =
                                (struct MMAL_PORT_USERDATA_T *)&callback_data;



    // Enable the encoder output port and tell it its callback function.

    status = mmal_port_enable(encoder->output[0], encoder_buffer_callback);
    if (status) {
        LOG_ERROR("can't mmal_port_enable(encoder_buffer_callback)\n");
        mmal_component_destroy(camera);
        if (encoder) mmal_component_destroy(encoder);
        exit(EXIT_FAILURE);
    }

    if (usestills) {
        DBG("Starting stills output\n");

        //setup fps
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        frames = 0;
        int delay = (1000 * 1000) / fps;

        while (!pglobal->stop) {
            //Wait the delay
            usleep(delay);

            // Send all the buffers to the encoder output port
            int num = mmal_queue_length(pool->queue);
            int q;
            for (q = 0; q < num; q++) {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
                if (!buffer) {
                    LOG_ERROR("can't mmal_queue_get(encoder_pool)\n");
                    exit(EXIT_FAILURE);
                }
                if (mmal_port_send_buffer(encoder->output[0], buffer)
                                                            != MMAL_SUCCESS) {
                    LOG_ERROR("can't send a buffer to encoder output port\n");
                    exit(EXIT_FAILURE);
                }
            }

            if (mmal_port_parameter_set_boolean(camera_still_port,
                                                MMAL_PARAMETER_CAPTURE, 1)
                                                            != MMAL_SUCCESS) {
                LOG_ERROR("can't start camera capture\n");
                exit(EXIT_FAILURE);
            } else {
                /* Wait for capture to complete.
                   For some reason using vcos_semaphore_wait_timeout sometimes
                   returns immediately with bad parameter error even though it
                   appears to be all correct, so reverting to untimed one
                   until figure out why its erratic. */
                vcos_semaphore_wait(&callback_data.complete_semaphore);
            }

            frames++;
            if (frames == 100) {
                //calculate fps
                clock_gettime(CLOCK_MONOTONIC, &t_finish);
                t_elapsed = (t_finish.tv_sec - t_start.tv_sec);
                t_elapsed += (t_finish.tv_nsec - t_start.tv_nsec) /
                                                                  1000000000.0;
                fprintf(stderr, "%i frames captured in %f seconds (%f fps)\n",
                        frames, t_elapsed, (frames / t_elapsed));
                frames = 0;
                clock_gettime(CLOCK_MONOTONIC, &t_start);
            }
        }
    } else {
        //Video Mode
        DBG("Starting video output\n");
        // Send all the buffers to the encoder output port
        int num = mmal_queue_length(pool->queue);
        int q;
        for (q = 0; q < num; q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
            if (!buffer) {
                LOG_ERROR("can't mmal_queue_get(encoder_pool buffer %d)\n", q);
            }
            if (mmal_port_send_buffer(encoder->output[0], buffer)
                                                            != MMAL_SUCCESS) {
                LOG_ERROR(
                    "can't mmal_port_send_buffer(encodeer_output_port (%d))\n",
                          q);
            }
        }
        if (splitter_callback_port != NULL) {
            int num = mmal_queue_length(splitter_callback_data.pool_ptr->queue);
            int q;
            for (q = 0; q < num; q++) {
                MMAL_BUFFER_HEADER_T *buffer =
                        mmal_queue_get(splitter_callback_data.pool_ptr->queue);

                if (!buffer) {
                    LOG_ERROR("can't mmal_queue_get(splitter_pool buffer %d)\n",
                              q);
                }

                if (mmal_port_send_buffer(splitter_callback_port, buffer)
                                                             != MMAL_SUCCESS) {
                    LOG_ERROR(
                    "can't mmal_port_send_buffer(spliiter_output_port (%d))\n",
                              q);
                }
            }
        }
        if (mmal_port_parameter_set_boolean(camera_video_port,
                                            MMAL_PARAMETER_CAPTURE, 1)
                                                             != MMAL_SUCCESS) {
            LOG_ERROR("can't start capture\n");
        }
        while(!pglobal->stop) usleep(1000);
    }

    vcos_semaphore_delete(&callback_data.complete_semaphore);
    if (splitter_callback_data.yuv_fp != NULL) {
        fclose(splitter_callback_data.yuv_fp);
    }

    // Close everything MMAL

    if (usestills) {
        if (camera_video_port && camera_video_port->is_enabled) {
            mmal_port_disable(camera_video_port);
        }
    } else {
        if (camera_still_port && camera_still_port->is_enabled) {
            mmal_port_disable(camera_still_port);
        }
    }
    if (preview_connection) mmal_connection_destroy(preview_connection);
    if (encoder->output[0] && encoder->output[0]->is_enabled) {
        mmal_port_disable(encoder->output[0]);
    }
    mmal_connection_destroy(encoder_connection);

    // Disable components

    if (encoder) mmal_component_disable(encoder);
    if (preview) mmal_component_disable(preview);
    if (camera) mmal_component_disable(camera);

    // Destroy encoder component.  Get rid of any port buffers first

    if (pool) {
        mmal_port_pool_destroy(encoder->output[0], pool);
    }

    if (encoder) {
        mmal_component_destroy(encoder);
        encoder = NULL;
    }
    if (preview) {
        mmal_component_destroy(preview);
        preview = NULL;
    }

    // Destroy camera component.

    if (camera) {
        mmal_component_destroy(camera);
        camera = NULL;
    }

    DBG("mmal cleanup done\n");
    pthread_cleanup_pop(1);
    return NULL;
}

/******************************************************************************
  Description.: this functions cleans up allocated resources
  Input Value.: arg is unused
  Return Value: -
 ******************************************************************************/
void worker_cleanup(void *arg) {
    static unsigned char first_run = 1;
    if (!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    DBG("cleaning up resources allocated by input thread\n");

    if (pglobal->in[plugin_number].buf != NULL) {
        free(pglobal->in[plugin_number].buf);
    }
}
