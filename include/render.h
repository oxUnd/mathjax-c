#ifndef MJX_RENDER_H
#define MJX_RENDER_H

#include "font.h"
#include "layout.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixel buffer */
typedef struct mjx_buf {
  unsigned int width;
  unsigned int height;
  uint32_t* pixels;      /* RGBA: 0xRRGGBBAA */
  uint32_t bg_color;     /* background color (0 = transparent) */
  uint32_t fg_color;     /* foreground color */
} mjx_buf;

/* Renderer context */
typedef struct mjx_renderer {
  mjx_font* font;
  mjx_font* fallback_font;
  uint32_t fg_color;
  uint32_t bg_color;
} mjx_renderer;

/* Create/destroy renderer */
mjx_renderer* mjx_renderer_create(mjx_font* font);
void          mjx_renderer_destroy(mjx_renderer* r);

/* Allocate pixel buffer */
mjx_buf* mjx_buf_alloc(unsigned int width, unsigned int height, uint32_t bg);

/* Free pixel buffer */
void mjx_buf_free(mjx_buf* buf);

/* Render a box tree to a pixel buffer */
mjx_buf* mjx_render_box(mjx_renderer* r, mjx_box* box);

/* Low-level drawing operations */
void mjx_buf_clear(mjx_buf* buf, uint32_t color);
void mjx_buf_set_pixel(mjx_buf* buf, int x, int y, uint32_t color);
void mjx_buf_fill_rect(mjx_buf* buf, int x, int y, int w, int h, uint32_t color);
void mjx_buf_blit_alpha(mjx_buf* buf, const unsigned char* alpha,
                         int x, int y, int w, int h,
                         uint32_t color, int left, int top);
void mjx_buf_draw_line(mjx_buf* buf, int x1, int y1, int x2, int y2,
                        double thickness, uint32_t color);

/* Get/set pixel */
uint32_t mjx_buf_get_pixel(const mjx_buf* buf, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* MJX_RENDER_H */
