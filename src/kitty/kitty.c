#include "kitty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * Kitty Graphics Protocol implementation.
 * 
 * Sends image data using the Kitty terminal graphics protocol:
 *   ESC _ G <params> ; <data> ESC \
 *
 * For direct RGBA transmission we use the 'a' (transmission = direct) 
 * and 'f' (format = 32 for RGBA) parameters.
 */

/* Base64 encoding */
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(const unsigned char* in, size_t in_len, char* out, size_t* out_len) {
  size_t i = 0, j = 0;
  while (i < in_len) {
    unsigned int octet_a = i < in_len ? in[i++] : 0;
    unsigned int octet_b = i < in_len ? in[i++] : 0;
    unsigned int octet_c = i < in_len ? in[i++] : 0;
    unsigned int triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    out[j++] = b64[(triple >> 18) & 0x3F];
    out[j++] = b64[(triple >> 12) & 0x3F];
    out[j++] = b64[(triple >> 6) & 0x3F];
    out[j++] = b64[triple & 0x3F];
  }

  /* Padding */
  size_t padding = (3 - in_len % 3) % 3;
  for (size_t p = 0; p < padding; p++) {
    out[j - 1 - p] = '=';
  }

  *out_len = j;
}

void mjx_kitty_send(const char* data, size_t len) {
  fwrite(data, 1, len, stdout);
  fflush(stdout);
}

void mjx_kitty_flush(void) {
  fflush(stdout);
}

int mjx_kitty_query_support(void) {
  /* Kitty protocol query: request response from terminal */
  printf("\033_Gi=31,s=1,v=1\033\\");
  fflush(stdout);
  /* In a real implementation we'd read the response from stdin.
   * For now, assume Kitty-compatible terminal. */
  return 1;
}

mjx_error mjx_kitty_send_rgba(const unsigned char* rgba,
                               unsigned int width, unsigned int height) {
  size_t pixel_count = (size_t)width * height;

  /* Send RGBA data directly (format=32 for RGBA, transmission='d' for direct) */
  /* We need to chunk the data, max chunk size is typically 4096 bytes */

  size_t chunk_size = 1024;  /* safe chunk size for escape sequences */
  size_t offset = 0;
  int first_chunk = 1;

  while (offset < pixel_count * 4) {
    size_t remaining = pixel_count * 4 - offset;
    size_t this_chunk = remaining < chunk_size ? remaining : chunk_size;

    /* Encode chunk to base64 */
    size_t b64_len = ((this_chunk + 2) / 3) * 4 + 1;
    char* b64_data = (char*)malloc(b64_len);
    if (!b64_data) return MJX_ERR_NOMEM;

    b64_encode(rgba + offset, this_chunk, b64_data, &b64_len);

    /* Emit escape sequence */
    if (first_chunk) {
      printf("\033_Ga=T,f=32,s=%u,v=%u,m=%d;", width, height,
             (offset + this_chunk < pixel_count * 4) ? 1 : 0);
      first_chunk = 0;
    } else {
      printf("\033_Gm=%d;",
             (offset + this_chunk < pixel_count * 4) ? 1 : 0);
    }

    fwrite(b64_data, 1, b64_len, stdout);
    printf("\033\\");
    fflush(stdout);

    free(b64_data);
    offset += this_chunk;
  }

  return MJX_OK;
}

mjx_error mjx_kitty_display(const mjx_buf* buf, const mjx_kitty_opts* opts) {
  if (!buf || !buf->pixels) return MJX_ERR_INVALID_ARG;

  /* Convert RGBA buffer (uint32_t) to raw RGBA bytes */
  size_t pixel_count = (size_t)buf->width * buf->height;
  unsigned char* rgba = (unsigned char*)malloc(pixel_count * 4);
  if (!rgba) return MJX_ERR_NOMEM;

  for (size_t i = 0; i < pixel_count; i++) {
    uint32_t p = buf->pixels[i];
    rgba[i * 4 + 0] = (p >> 24) & 0xFF;  /* R */
    rgba[i * 4 + 1] = (p >> 16) & 0xFF;  /* G */
    rgba[i * 4 + 2] = (p >> 8) & 0xFF;   /* B */
    rgba[i * 4 + 3] = (p >> 0) & 0xFF;   /* A */
  }

  mjx_error err = mjx_kitty_send_rgba(rgba, buf->width, buf->height);
  free(rgba);

  /* Move cursor below the image */
  printf("\n");
  fflush(stdout);

  return err;
}
