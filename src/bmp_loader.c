#include "bmp_loader.h"
#include "vga.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t rgb555_lut[32 * 32 * 32];
static int lut_ready = 0;

void bmp_init_quantizer(void) {
    int r5, g5, b5, i;
    for (r5 = 0; r5 < 32; r5++) {
        int r6 = (r5 * 63) / 31;
        for (g5 = 0; g5 < 32; g5++) {
            int g6 = (g5 * 63) / 31;
            for (b5 = 0; b5 < 32; b5++) {
                int b6 = (b5 * 63) / 31;
                int best_idx = 0;
                int best_dist = 0x7FFFFFFF;

                for (i = 0; i < 255; i++) {
                    int dr = r6 - (int)vga_palette[i][0];
                    int dg = g6 - (int)vga_palette[i][1];
                    int db = b6 - (int)vga_palette[i][2];
                    int dist = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_idx = i;
                        if (dist == 0) break;
                    }
                }
                rgb555_lut[(r5 << 10) | (g5 << 5) | b5] = (uint8_t)best_idx;
            }
        }
    }
    lut_ready = 1;
}

uint8_t bmp_rgb_to_vga(uint8_t r, uint8_t g, uint8_t b) {
    /* Treat Hot Magenta (#FF00FF) as transparent sprite key */
    if (r >= 245 && g <= 15 && b >= 245) {
        return COL_TRANS;
    }
    if (!lut_ready) bmp_init_quantizer();
    return rgb555_lut[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)];
}

static FILE *open_asset_file(const char *name) {
    char path[128];
    FILE *fp = fopen(name, "rb");
    if (fp) return fp;
    snprintf(path, sizeof(path), "assets/%s", name);
    fp = fopen(path, "rb");
    if (fp) return fp;
    snprintf(path, sizeof(path), "../assets/%s", name);
    return fopen(path, "rb");
}

uint8_t *bmp_load_alloc(const char *name, int *out_w, int *out_h) {
    FILE *fp = open_asset_file(name);
    uint8_t hdr[54];
    uint32_t data_offset, dib_size, compression;
    int32_t width, height;
    uint16_t bpp;
    int abs_h, flip_y, row_stride, y, x;
    uint8_t *dst = NULL;
    uint8_t *row_buf = NULL;
    uint8_t bmp_pal[256][4];

    if (!fp) return NULL;
    if (fread(hdr, 1, 54, fp) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        fclose(fp);
        return NULL;
    }

    memcpy(&data_offset, &hdr[10], 4);
    memcpy(&dib_size,    &hdr[14], 4);
    memcpy(&width,       &hdr[18], 4);
    memcpy(&height,      &hdr[22], 4);
    memcpy(&bpp,         &hdr[28], 2);
    memcpy(&compression, &hdr[30], 4);

    if (width <= 0 || width > 2048 || height == 0 || abs(height) > 2048 || compression != 0) {
        fclose(fp);
        return NULL;
    }
    if (bpp != 8 && bpp != 24 && bpp != 32) {
        fclose(fp);
        return NULL;
    }

    abs_h  = (height > 0) ? height : -height;
    flip_y = (height > 0); /* Standard BMP stores bottom row first */

    if (bpp == 8) {
        /* Read 8-bit color table right after DIB header */
        fseek(fp, 14 + (long)dib_size, SEEK_SET);
        memset(bmp_pal, 0, sizeof(bmp_pal));
        fread(bmp_pal, 4, 256, fp);
    }

    row_stride = ((width * (bpp / 8)) + 3) & ~3;
    row_buf = (uint8_t *)malloc((size_t)row_stride);
    dst     = (uint8_t *)malloc((size_t)(width * abs_h));
    if (!row_buf || !dst) {
        free(row_buf);
        free(dst);
        fclose(fp);
        return NULL;
    }

    fseek(fp, (long)data_offset, SEEK_SET);

    for (y = 0; y < abs_h; y++) {
        int dst_y = flip_y ? (abs_h - 1 - y) : y;
        uint8_t *dst_row = dst + dst_y * width;
        if (fread(row_buf, 1, (size_t)row_stride, fp) != (size_t)row_stride) break;

        if (bpp == 24) {
            for (x = 0; x < width; x++) {
                uint8_t b = row_buf[x * 3 + 0];
                uint8_t g = row_buf[x * 3 + 1];
                uint8_t r = row_buf[x * 3 + 2];
                dst_row[x] = bmp_rgb_to_vga(r, g, b);
            }
        } else if (bpp == 32) {
            for (x = 0; x < width; x++) {
                uint8_t b = row_buf[x * 4 + 0];
                uint8_t g = row_buf[x * 4 + 1];
                uint8_t r = row_buf[x * 4 + 2];
                uint8_t a = row_buf[x * 4 + 3];
                dst_row[x] = (a < 128) ? COL_TRANS : bmp_rgb_to_vga(r, g, b);
            }
        } else if (bpp == 8) {
            for (x = 0; x < width; x++) {
                uint8_t idx = row_buf[x];
                dst_row[x] = bmp_rgb_to_vga(bmp_pal[idx][2], bmp_pal[idx][1], bmp_pal[idx][0]);
            }
        }
    }

    free(row_buf);
    fclose(fp);
    if (out_w) *out_w = width;
    if (out_h) *out_h = abs_h;
    return dst;
}

void bmp_extract_tile(const uint8_t *sheet, int sheet_w, int sheet_h,
                      int sx, int sy, int w, int h, uint8_t *dst) {
    int y, x;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int px = sx + x;
            int py = sy + y;
            if (px >= 0 && px < sheet_w && py >= 0 && py < sheet_h) {
                dst[y * w + x] = sheet[py * sheet_w + px];
            } else {
                dst[y * w + x] = COL_TRANS;
            }
        }
    }
}
