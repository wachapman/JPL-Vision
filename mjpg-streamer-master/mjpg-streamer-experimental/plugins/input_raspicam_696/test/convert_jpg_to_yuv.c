#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "yuv420.h"
#include "jpeg_file_io.h"

static char* remove_extension(char* fn) {
    char* dot_ptr = strrchr(fn, '.');
    if (dot_ptr != NULL) *dot_ptr = '\0';
    return fn;
}

static unsigned char* crop_to_even_cols_rows(unsigned int* cols_ptr,
                                             unsigned int* rows_ptr,
                                             unsigned char* rgb) {
    unsigned int cols = *cols_ptr;
    unsigned int rows = *rows_ptr;
    if (cols % 2 != 0) {
        unsigned int ii;
        unsigned int jj;
        unsigned int old_stride = 3 * cols;
        unsigned int new_stride = 3 * (cols - 1);
        unsigned char* new_rgb = (unsigned char*)malloc(new_stride * rows);
        for (ii = 0; ii < rows; ++ii) {
            for (jj = 0; jj < cols - 1; ++jj) {
                new_rgb[ii * new_stride + 3 * jj + 0] =
                                            rgb[ii * old_stride + 3 * jj + 0];
                new_rgb[ii * new_stride + 3 * jj + 1] =
                                            rgb[ii * old_stride + 3 * jj + 1];
                new_rgb[ii * new_stride + 3 * jj + 2] =
                                            rgb[ii * old_stride + 3 * jj + 2];
            }
        }
        --cols;
        free(rgb);
        rgb = new_rgb;
    }
    rows = rows / 2 * 2;
    *cols_ptr = cols;
    *rows_ptr = rows;
    return rgb;
}


int main(int argc, char* argv[]) {
    int return_val = -1;
    if (argc != 2) {
        fprintf(stderr, "usage: convert_jpg_to_jpg in_file.jpg\n");
        return -1;
    }
    char* in_filename = argv[1];

    char prefix[FILENAME_MAX];
    strncpy(prefix, basename(in_filename), FILENAME_MAX);
    (void)remove_extension(prefix);

    unsigned int cols;
    unsigned int rows;
    unsigned char* rgb = jpeg_file_read(in_filename, &cols, &rows);
    if (rgb == NULL) {
        fprintf(stderr, "can't read %s\n", in_filename);
        return -1;
    }
    rgb = crop_to_even_cols_rows(&cols, &rows, rgb);
    unsigned int bytes = cols * rows * 3 / 2;
    unsigned char* yuv = (unsigned char*)malloc(bytes);
    convert_rgb_to_yuv420(cols, rows, rgb, bytes, yuv);
    char out_filename[FILENAME_MAX];
    snprintf(out_filename, FILENAME_MAX, "%s.yuv", prefix);
    if (yuv420_write(out_filename, cols, rows, yuv) < 0) {
        fprintf(stderr, "can't write to %s\n", out_filename);
    } else {
        return_val = 0;
    }
    free(yuv);
    free(rgb);
    return return_val;
}
