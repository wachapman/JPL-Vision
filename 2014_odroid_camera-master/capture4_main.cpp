/**********************************************************************
 * Placed in the public domain by the author, Daniel Clouse, November 15, 2014.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "usb_camera.h"
#include "cam_thread.h"

pthread_mutex_t disp_mutex = PTHREAD_MUTEX_INITIALIZER;

const int CAM_COUNT = 2;
Usb_Camera cam[CAM_COUNT];

int main()
{
    /* Looks like this code will run:
       one 480x640 camera at about 43 fps.
       two 480x640 cameras at 15 fps.
       one 240x320 camera at 125 fps.
       one 480x640 and one 240x320 camera at about 30 fps.
     */
    pthread_t thread_id[CAM_COUNT];

    cam[0].init("/dev/video10", 2, 240, 320, 5);
    cam[0].set_frame_interval(1, 60);

    //cam[0].init("/dev/video10", 2, 480, 640, 5);
    //cam[0].set_frame_interval(1, 30);
    //cam[1].init("/dev/video11", 2, 480, 640, 5);
/*
    cam[1].init("/dev/video11", 2, 240, 320, 5);
    cam[1].set_frame_interval(1, 40);

    int cam_count = 2;
*/
    int cam_count = 1;
    for (int i = 0; i < cam_count; ++i) {
        const int STR_BYTES = 81;
        char str[STR_BYTES];
        printf("%s FORMATS\n--------------------\n", cam[i].get_device_name());
        fflush(stdout);
        int format_id_in = 0;
        while (1) {
            const struct v4l2_fmtdesc* fmt_desc_ptr;
            int format_id_out = cam[i].get_format(format_id_in, fmt_desc_ptr);
            if (format_id_out != format_id_in) break;
            printf("  %s\n",
                   Usb_Camera::format_str(*fmt_desc_ptr, STR_BYTES, str));
            fflush(stdout);
            ++format_id_in;
        }

        const int MAX_IVAL = 10;
        struct v4l2_frmivalenum frm_ival[MAX_IVAL];
        int format_id = cam[i].get_current_format_id();
        int curr_rows = cam[i].get_rows();
        int curr_cols = cam[i].get_cols();
        int ival_count = cam[i].get_supported_frame_intervals(
                                    format_id, curr_rows, curr_cols,
                                    MAX_IVAL, frm_ival);
        printf("%s FRAME_INVERVALS\n--------------------\n",
               cam[i].get_device_name());
        for (int j = 0; j < ival_count; ++j) {
            printf("  %s\n",
               Usb_Camera::frame_interval_str(frm_ival[j], STR_BYTES, str));
        }
    }

    for (int i = 0; i < cam_count; ++i) {
        int rc = pthread_create(&thread_id[i], NULL, cam_thread,
                                (void*)&cam[i]);
        if (rc != 0) {
            printf("can't pthread_create, error_code= %d\n", rc);
            exit(-1);
        }
    }
    pthread_exit(NULL);
}
