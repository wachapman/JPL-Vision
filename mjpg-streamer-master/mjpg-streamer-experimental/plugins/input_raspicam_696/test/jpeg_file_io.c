#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <jpeglib.h>
#include "jpeg_file_io.h"

unsigned char* jpeg_file_read(const char* filename,
                              unsigned int* width_ptr,
                              unsigned int* height_ptr) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "can't open %s for reading\n", filename);
        return NULL;
    }

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

int jpeg_file_write(const char* filename,
                    unsigned int quality,
                    unsigned int width,
                    unsigned int height,
                    unsigned char rgb[]) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) return -1;
    jpeg_stdio_dest(&cinfo, fp);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, true);
    jpeg_start_compress(&cinfo, true);
    int row_stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &rgb[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    fclose(fp);
    jpeg_destroy_compress(&cinfo);
}
