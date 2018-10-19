unsigned char* jpeg_file_read(const char* filename,
                              unsigned int* width_ptr,
                              unsigned int* height_ptr);
int jpeg_file_write(const char* filename,
                    unsigned int quality,
                    unsigned int width,
                    unsigned int height,
                    unsigned char rgb[]);
