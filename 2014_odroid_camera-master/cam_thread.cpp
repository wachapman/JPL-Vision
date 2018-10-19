/**********************************************************************
 * Placed in the public domain by the author, Daniel Clouse, November 15, 2014.
 */
#include <pthread.h>
#include <time.h>
#include <opencv2/opencv.hpp>
#include "frame_queue.h"
#include "cam_thread.h"

extern pthread_mutex_t disp_mutex;

class Thread_Info {
public:
    Any_Frame_Queue* in_queue_ptr;
    Any_Frame_Queue* out_queue_ptr;
    Usb_Camera* cam_ptr;
};

static double tv_subtract(const struct timeval& a, const struct timeval& b)
{
    const long USEC_PER_SECOND = 1000000;
    suseconds_t usec_diff = a.tv_usec - b.tv_usec;
    time_t sec_diff = a.tv_sec - b.tv_sec;
    if (usec_diff < 0) {
        usec_diff += USEC_PER_SECOND;
        sec_diff -= 1;
    }
    return sec_diff + usec_diff / (double)USEC_PER_SECOND;
}

static double ts_subtract(const struct timespec& a, const struct timespec& b)
{
    const long NSEC_PER_SECOND = 1000000000;
    long nsec_diff = a.tv_nsec - b.tv_nsec;
    time_t sec_diff = a.tv_sec - b.tv_sec;
    if (nsec_diff < 0) {
        nsec_diff += NSEC_PER_SECOND;
        sec_diff -= 1;
    }
    return sec_diff + nsec_diff / (double)NSEC_PER_SECOND;
}

static void* capture_thread(void* thread_arg_ptr)
{
    Thread_Info* iptr = (Thread_Info*)thread_arg_ptr;
    Usb_Camera* cam_ptr = iptr->cam_ptr;
    cam_ptr->stream_start();
    struct timeval start_time;
    struct timespec start_cpu_time;
    gettimeofday(&start_time, NULL);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_cpu_time);
    while (1) {
        int in_count;
        Usb_Frame* frame_ptr = iptr->in_queue_ptr->pop(in_count);

        struct timeval now;
        struct timespec now_cpu_time;
        gettimeofday(&now, NULL);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now_cpu_time);
        double secs = tv_subtract(now, start_time);
        double cpu_secs = ts_subtract(now_cpu_time, start_cpu_time);
        struct timeval tv = frame_ptr->get_timestamp();
        int out_count = iptr->out_queue_ptr->push(frame_ptr);
        printf("capture %s: in=%d out=%d frame=%7d time=%10ld.%06ld cpu=%.6f (%3d%%)\n",
               cam_ptr->get_device_name(), in_count, out_count,
               frame_ptr->get_frame_num(), tv.tv_sec, tv.tv_usec, cpu_secs,
               (int)(cpu_secs / secs * 100.0 + 0.5));

    }
    return NULL;
}

static void* display_thread(void* thread_arg_ptr)
{
    Thread_Info* iptr = (Thread_Info*)thread_arg_ptr;
    const char* dev_name = iptr->cam_ptr->get_device_name();
    struct timeval start_time;
    struct timespec start_cpu_time;
    gettimeofday(&start_time, NULL);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_cpu_time);
    while (1) {
        int in_count;
        Usb_Frame* frame_ptr = iptr->in_queue_ptr->pop(in_count);
        cv::Mat image(frame_ptr->get_rows(), frame_ptr->get_cols(),
                      CV_8UC3, frame_ptr->get_img_data());

        pthread_mutex_lock(&disp_mutex);
        cv::namedWindow(dev_name, 1);
        pthread_mutex_unlock(&disp_mutex);

        cv::imshow(dev_name, image);
        cv::waitKey(1);

        struct timeval now;
        struct timespec now_cpu_time;
        gettimeofday(&now, NULL);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now_cpu_time);
        double secs = tv_subtract(now, start_time);
        double cpu_secs = ts_subtract(now_cpu_time, start_cpu_time);
        struct timeval tv = frame_ptr->get_timestamp();
        int out_count = iptr->out_queue_ptr->push(frame_ptr);
        printf("display %s: in=%d out=%d frame=%7d time=%10ld.%06ld cpu=%.6f (%3d%%)\n",
               dev_name, in_count, out_count, frame_ptr->get_frame_num(),
               tv.tv_sec, tv.tv_usec, cpu_secs,
               (int)(cpu_secs / secs * 100.0 + 0.5));

    }
    return NULL;
}

void* cam_thread(void* thread_arg_ptr)
{
    Usb_Camera* cam_ptr = (Usb_Camera*)thread_arg_ptr;
    int buf_count = cam_ptr->get_buf_count();
    printf("buf_count= %d\n", buf_count);
    Frame_Queue* q1_ptr = new Frame_Queue(buf_count, true, true);

    Thread_Info display_thread_info;
    display_thread_info.in_queue_ptr = q1_ptr;
    display_thread_info.out_queue_ptr = cam_ptr;
    display_thread_info.cam_ptr = cam_ptr;

    pthread_t display_thread_id;
    int rc = pthread_create(&display_thread_id, NULL, display_thread,
                            (void*)&display_thread_info);
    if (rc != 0) {
        printf("can't pthread_create, error_code= %d\n", rc);
        exit(-1);
    }

    // don't start a new thread for capture_thread; just morph this one.

    Thread_Info capture_thread_info;
    capture_thread_info.in_queue_ptr = cam_ptr;
    capture_thread_info.out_queue_ptr = q1_ptr;
    capture_thread_info.cam_ptr = cam_ptr;
    void* return_val = capture_thread(&capture_thread_info);

    delete q1_ptr;
    return return_val;
}
