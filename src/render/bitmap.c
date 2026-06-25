#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

mjx_buf* mjx_buf_alloc(unsigned int width, unsigned int height, uint32_t bg) {
  mjx_buf* buf = (mjx_buf*)calloc(1, sizeof(mjx_buf));
  if (!buf) return NULL;

  buf->pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
  if (!buf->pixels) {
    free(buf);
    return NULL;
  }

  buf->width = width;
  buf->height = height;
  buf->bg_color = bg;
  buf->fg_color = 0x000000FF;  /* default: black, fully opaque */

  if (bg) {
    mjx_buf_clear(buf, bg);
  }

  return buf;
}

void mjx_buf_free(mjx_buf* buf) {
  if (!buf) return;
  free(buf->pixels);
  free(buf);
}

void mjx_buf_clear(mjx_buf* buf, uint32_t color) {
  if (!buf || !buf->pixels) return;
  for (unsigned int i = 0; i < buf->width * buf->height; i++) {
    buf->pixels[i] = color;
  }
}

void mjx_buf_set_pixel(mjx_buf* buf, int x, int y, uint32_t color) {
  if (!buf || !buf->pixels) return;
  if (x < 0 || (unsigned int)x >= buf->width) return;
  if (y < 0 || (unsigned int)y >= buf->height) return;
  buf->pixels[y * buf->width + x] = color;
}

uint32_t mjx_buf_get_pixel(const mjx_buf* buf, int x, int y) {
  if (!buf || !buf->pixels) return 0;
  if (x < 0 || (unsigned int)x >= buf->width) return 0;
  if (y < 0 || (unsigned int)y >= buf->height) return 0;
  return buf->pixels[y * buf->width + x];
}

void mjx_buf_draw_line(mjx_buf* buf, int x1, int y1, int x2, int y2,
                        double thickness, uint32_t color) {
  if (!buf || !buf->pixels || thickness <= 0) return;
  int dx = x2 - x1;
  int dy = y2 - y1;
  if (dx == 0 && dy == 0) {
    mjx_buf_set_pixel(buf, x1, y1, color);
    return;
  }
  int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
  double half_t = thickness / 2.0;
  double t_x = (dy != 0) ? -half_t * dx / sqrt((double)(dx*dx + dy*dy)) : half_t;
  double t_y = (dx != 0) ? half_t * dy / sqrt((double)(dx*dx + dy*dy)) : half_t;
  for (int i = 0; i <= steps; i++) {
    double t = (double)i / (double)steps;
    double cx = x1 + t * dx;
    double cy = y1 + t * dy;
    if (thickness <= 1.5) {
      mjx_buf_set_pixel(buf, (int)(cx + 0.5), (int)(cy + 0.5), color);
    } else {
      int bx = (int)(cx - half_t + 0.5);
      int by = (int)(cy - half_t + 0.5);
      mjx_buf_fill_rect(buf, bx, by, (int)(thickness + 0.5), (int)(thickness + 0.5), color);
    }
  }
}

void mjx_buf_fill_rect(mjx_buf* buf, int x, int y, int w, int h, uint32_t color) {
  if (!buf || !buf->pixels) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > (int)buf->width) w = buf->width - x;
  if (y + h > (int)buf->height) h = buf->height - y;
  if (w <= 0 || h <= 0) return;

  for (int row = 0; row < h; row++) {
    uint32_t* row_start = buf->pixels + (y + row) * buf->width + x;
    for (int col = 0; col < w; col++) {
      row_start[col] = color;
    }
  }
}

/* Composite an alpha-masked glyph onto the buffer */
void mjx_buf_blit_alpha(mjx_buf* buf, const unsigned char* alpha,
                         int x, int y, int w, int h,
                         uint32_t color, int left, int top) {
  if (!buf || !alpha) return;

  int src_w = w;
  int dest_x = x + left;
  int dest_y = y - (top - h);  /* convert from glyph coordinates */

  /* Clipping */
  int src_off_x = 0;
  int src_off_y = 0;
  if (dest_x < 0) { src_off_x = -dest_x; w += dest_x; dest_x = 0; }
  if (dest_y < 0) { src_off_y = -dest_y; h += dest_y; dest_y = 0; }
  if (dest_x + w > (int)buf->width) w = buf->width - dest_x;
  if (dest_y + h > (int)buf->height) h = buf->height - dest_y;
  if (w <= 0 || h <= 0) return;

  uint8_t fr = (color >> 24) & 0xFF;
  uint8_t fg = (color >> 16) & 0xFF;
  uint8_t fb = (color >> 8) & 0xFF;

  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      int src_idx = (src_off_y + row) * src_w + (src_off_x + col);
      uint8_t a = alpha[src_idx];
      if (a == 0) continue;

      uint32_t* dst = &buf->pixels[(dest_y + row) * buf->width + (dest_x + col)];
      uint8_t dr = (*dst >> 24) & 0xFF;
      uint8_t dg = (*dst >> 16) & 0xFF;
      uint8_t db = (*dst >> 8) & 0xFF;
      uint8_t da = (*dst >> 0) & 0xFF;

      if (a == 255) {
        *dst = ((uint32_t)fr << 24) | ((uint32_t)fg << 16) |
               ((uint32_t)fb << 8) | 0xFFu;
      } else {
        uint8_t nr = (fr * a + dr * (255 - a)) / 255;
        uint8_t ng = (fg * a + dg * (255 - a)) / 255;
        uint8_t nb = (fb * a + db * (255 - a)) / 255;
        uint8_t na = da + (255 - da) * a / 255;
        *dst = ((uint32_t)nr << 24) | ((uint32_t)ng << 16) |
               ((uint32_t)nb << 8) | (uint32_t)na;
      }
    }
  }
}
