#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <jpeglib.h>
#include "detect_color_blobs.h"
#include "yuv420.h"

static unsigned char* jpeg_read(const char* filename,
                                unsigned int* width_ptr,
                                unsigned int* height_ptr) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) return NULL;

    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    (void)jpeg_read_header(&cinfo, true);
    (void)jpeg_start_decompress(&cinfo);
    int width = cinfo.output_width;
    int height = cinfo.output_height;

    unsigned char *buf = (unsigned char*)malloc(width * height * 3);
    unsigned char *p = buf;
    int row_stride = width * cinfo.output_components;
    JSAMPARRAY jpeg_buf = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                                     JPOOL_IMAGE, row_stride,
                                                     1);
    while (cinfo.output_scanline < cinfo.output_height) {
        (void) jpeg_read_scanlines(&cinfo, jpeg_buf, 1);
        int x;
        for (x = 0; x < width; x++) {
            unsigned char g;
            unsigned char b;
            unsigned char r = jpeg_buf[0][cinfo.output_components * x];
            if (cinfo.output_components > 2) {
                g = jpeg_buf[0][cinfo.output_components * x + 1];
                b = jpeg_buf[0][cinfo.output_components * x + 2];
            } else {
                g = r;
                b = r;
            }
            *(p++) = r;
            *(p++) = g;
            *(p++) = b;
        }
    }
    fclose(fp);
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    *width_ptr = width;
    *height_ptr = height;
    return buf;
}

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

static int convert_rgb_to_yuv440(unsigned int width,
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
    for (ii = 0; ii < height / 4; ++ii) {
        for (jj = 0; jj < width / 4; ++jj) {
            unsigned int yno_00 = ii * (2 * width) + 2 * jj;
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

            unsigned int uvno = ii * width + jj;
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
    return required_yuv_bytes;
}

static double delta_time(struct timespec* a_ptr, struct timespec* b_ptr) {
    return (b_ptr->tv_sec - a_ptr->tv_sec) +
           (b_ptr->tv_nsec - a_ptr->tv_nsec) / 1000000000.0;
}

static void dump_root_info_stats(Blob_List* p, int index) {
    const Blob_Stats* s = &p->root_info[index].stats;
    fprintf(stderr,
"root %3hu: bbox (%5d %5d) (%5d %5d) cnt %6d sum (%7lu %7lu) center (%5lu %5lu)\n",
            index, s->min_x, s->min_y, s->max_x, s->max_y, s->count,
            s->sum_x, s->sum_y,
            (s->sum_x + s->count / 2) / s->count,
            (s->sum_y + s->count / 2) / s->count);
}

int main(int argc, const char* argv[]) {
    if (argc != 8) {
        fprintf(stderr,
                "usage: img_in y_low y_high u_low u_high v_low v_high\n");
        return -1;
    }

    const char* in_fn = argv[1];
    int y_low = atoi(argv[2]);
    //int y_high = atoi(argv[3]);
    int u_low = atoi(argv[4]);
    int u_high = atoi(argv[5]);
    int v_low = atoi(argv[6]);
    int v_high = atoi(argv[7]);

    unsigned int cols = 0;
    unsigned int rows = 0;
    unsigned int yuv_bytes;
    unsigned char* yuv;
    size_t in_fn_len = strlen(in_fn);
    if (in_fn_len < 4 || strcmp(&in_fn[in_fn_len - 4], ".yuv") == 0) {
        // This is a .yuv file.

        // Read the .yuv file.

        yuv = yuv420_read(in_fn, &cols, &rows);
        if (yuv == NULL) {
            fprintf(stderr, "can't read %s\n", in_fn);
            return -1;
        }
    } else {
        // Assume it's a .jpg file.

        // Read the .jpg file.

        unsigned char* rgb = jpeg_read(in_fn, &cols, &rows);
        if (rgb == NULL) {
            fprintf(stderr, "can't read %s\n", in_fn);
            return -1;
        }

        // Convert it to YUV440 format.

        unsigned int pixels = cols * rows;
        yuv_bytes = pixels * 3 / 2;
        yuv = (unsigned char*)malloc(yuv_bytes);
        convert_rgb_to_yuv440(cols, rows, rgb, yuv_bytes, yuv);
        free(rgb);
    }

    struct timespec start_time;
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &start_time);
#define MAX_RUNS 10000
#define MAX_BLOBS 1000
    Blob_List bl = blob_list_init(MAX_RUNS, MAX_BLOBS);
#ifdef DCB_DEBUG
    bool highlight_detected_pixels = true;
#else
    bool highlight_detected_pixels = false;
#endif
    detect_color_blobs(&bl, y_low, u_low, u_high, v_low, v_high,
                       highlight_detected_pixels, cols, rows, yuv);
    clock_gettime(CLOCK_REALTIME, &end_time);

    int ii;
    fprintf(stderr, "All blobs:\n");
    for (ii = 0; ii < bl.used_root_list_count; ++ii) {
        dump_root_info_stats(&bl, ii);
    }
    sort_blobs_by_pixel_count(&bl);

    fprintf(stderr, "Sorted blobs:\n");
    for (ii = 0; ii < bl.used_root_list_count; ++ii) {
        dump_root_info_stats(&bl, ii);
    }

    unsigned char red[3] = { 255, 0, 255 };
    draw_bounding_boxes(&bl, 30, red, cols, rows, yuv);

    double elapsed_secs = delta_time(&start_time, &end_time);
    fprintf(stderr,
            "cr=( %d %d ) total_pixels= %lu elapsed_secs= %.6f %.3f Hz\n",
            cols, rows, get_total_blob_pixel_count(&bl), elapsed_secs,
            1.0 / elapsed_secs);
    blob_list_deinit(&bl);

    if (yuv420_write("bbox.yuv", cols, rows, yuv) < 0) {
        fprintf(stderr, "can't open out.yuv for writing\n");
        return -1;
    }
    free(yuv);
    return 0;
}
