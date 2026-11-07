#ifndef BMP_LOADER_H
#define BMP_LOADER_H

#include <stdint.h>

/* Initialize fast RGB555 -> VGA 256-color palette quantizer LUT (call after vga_build_palette) */
void bmp_init_quantizer(void);

/* Map an 8-bit RGB (0..255) color to our 256-color VGA palette (Magenta 255,0,255 -> COL_TRANS) */
uint8_t bmp_rgb_to_vga(uint8_t r, uint8_t g, uint8_t b);

/* Load a 24-bit, 32-bit, or 8-bit uncompressed BMP file, allocating a quantized VGA pixel buffer.
 * Searches "assets/<name>" and "<name>". Caller frees returned buffer. Returns NULL if not found. */
uint8_t *bmp_load_alloc(const char *name, int *out_w, int *out_h);

/* Copy a sub-rectangle (sx, sy, w, h) from a loaded sheet into dst[w * h] */
void bmp_extract_tile(const uint8_t *sheet, int sheet_w, int sheet_h,
                      int sx, int sy, int w, int h, uint8_t *dst);

#endif /* BMP_LOADER_H */
