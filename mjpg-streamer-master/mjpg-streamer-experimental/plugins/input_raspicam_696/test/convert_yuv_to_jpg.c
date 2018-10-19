#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include "yuv420.h"
#include "jpeg_file_io.h"

static int write_one_image(const char* prefix,
                           int image_no,
                           unsigned int cols,
                           unsigned int rows,
                           unsigned char yuv[],
                           unsigned char rgb[]) {
    char filename[FILENAME_MAX];
    snprintf(filename, FILENAME_MAX, "%s_%04d.jpg", prefix, image_no);
    filename[FILENAME_MAX - 1] = '\0';
    convert_yuv420_to_rgb(cols, rows, yuv, rgb);
    int status = jpeg_file_write(filename, 80, cols, rows, rgb);
    if (status < 0) {
        fprintf(stderr, "can't write to %s\n", filename);
    }
    return status;
}

static char* remove_extension(char* fn) {
    char* dot_ptr = strrchr(fn, '.');
    if (dot_ptr != NULL) *dot_ptr = '\0';
    return fn;
}

int main(int argc, char* argv[]) {
    int return_val = -1;
    if (argc != 2) {
        fprintf(stderr, "usage: convert_yuv_to_jpg in_file.yuv\n");
        return -1;
    }
    char* in_filename = argv[1];

    char prefix[FILENAME_MAX];
    strncpy(prefix, basename(in_filename), FILENAME_MAX);
    (void)remove_extension(prefix);

    Yuv_File yuv_file = yuv420_open_read(in_filename);
    if (yuv420_is_null(&yuv_file)) {
        fprintf(stderr, "can't open %s for reading\n", in_filename);
        return -1;
    }
    unsigned int cols = yuv420_get_cols(&yuv_file);
    unsigned int rows = yuv420_get_rows(&yuv_file);
    printf("cols=%u rows=%u\n", cols, rows);
    unsigned char* yuv0 = yuv420_malloc(&yuv_file);
    unsigned char* yuv = yuv420_malloc(&yuv_file);
    unsigned char* rgb = (unsigned char*)malloc(cols * rows * 3);
    /* Try to read two image. */

    int status = yuv420_read_next(&yuv_file, yuv0);
    if (status < 0) {
        fprintf(stderr, "%s is empty\n", in_filename);
        goto quit;
    }
    status = yuv420_read_next(&yuv_file, yuv);

    if (status < 0) {
        /* There's only one image in the input file.  Write the 0th image.
           The output filename does not contain a number. */

        char filename[FILENAME_MAX];
        snprintf(filename, FILENAME_MAX, "%s.jpg", prefix);
        convert_yuv420_to_rgb(cols, rows, yuv0, rgb);
        if (jpeg_file_write(filename, 80, cols, rows, rgb) < 0) {
            fprintf(stderr, "can't write to %s\n", filename);
        } else {
            return_val = 0;
        }
        goto quit;
    }

    /* Write the 0th image. */

    if (write_one_image(prefix, 0, cols, rows, yuv0, rgb) < 0) {
        goto quit;
    }

    /* Write the rest of the images. */

    int image_no = 1;
    do {
        if (write_one_image(prefix, image_no, cols, rows, yuv, rgb) < 0) {
            goto quit;
        }
        ++image_no;
    } while (yuv420_read_next(&yuv_file, yuv) >= 0);
    return_val = 0;
quit:
    yuv420_close(&yuv_file);
    free(yuv0);
    free(yuv);
    free(rgb);
    return return_val;
}
