#include <stdio.h>
#include <stdlib.h>
#include "yuv420.h"
#include "yuv_color_space_image.h"
int main(int argc, const char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: yuv_color_space_image cols rows y_value\n");
        return -1;
    }
    int cols = atoi(argv[1]);
    int rows = atoi(argv[2]);
    int y_value = atoi(argv[3]);
    unsigned char* yuv = (unsigned char*)malloc(cols * rows * 3 / 2);
    yuv_color_space_image(cols, rows, y_value, yuv);

    yuv420_write("out.yuv", cols, rows, yuv);
    return 0;
}
