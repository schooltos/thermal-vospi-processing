#include "output_image.h"

#include <stdio.h>
#include "core/config.h"

int output_image_write_pgm(const char *path, const DisplayFrame *frame)
{
    FILE *fp;

    if ((path == NULL) || (frame == NULL)) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }

    if (fprintf(fp, "P5\n%d %d\n255\n", FRAME_WIDTH, FRAME_HEIGHT) < 0) {
        fclose(fp);
        return -1;
    }

    if (fwrite(frame->pixels, 1, FRAME_PIXEL_COUNT, fp) != FRAME_PIXEL_COUNT) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int output_image_write_pgm_indexed(const char *directory,
                                   unsigned long frame_number,
                                   const DisplayFrame *frame)
{
    char path[256];
    int written;

    if ((directory == NULL) || (frame == NULL)) {
        return -1;
    }

    written = snprintf(path, sizeof(path),
                       "%s/frame_%06lu.pgm",
                       directory,
                       frame_number);

    if ((written < 0) || ((size_t)written >= sizeof(path))) {
        return -1;
    }

    return output_image_write_pgm(path, frame);
}