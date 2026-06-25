#ifndef MJX_KITTY_H
#define MJX_KITTY_H

#include "mathjax.h"
#include "render.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Kitty Graphics Protocol implementation.
 * Sends RGBA pixel data to the terminal via the Kitty graphics protocol
 * escape sequences.
 */

/* Display RGBA buffer in terminal via Kitty protocol */
mjx_error mjx_kitty_display(const mjx_buf* buf, const mjx_kitty_opts* opts);

/* Query if terminal supports kitty protocol (returns 1 if yes) */
int mjx_kitty_query_support(void);

/* Send raw kitty escape sequence */
void mjx_kitty_send(const char* data, size_t len);

/* 
 * Helper: encode RGBA pixels as base64 PNG and send as kitty
 * image protocol. If buf is provided, it uses raw RGBA directly
 * (compressed with zlib if available), otherwise encodes PNG.
 */
mjx_error mjx_kitty_send_rgba(const unsigned char* rgba,
                               unsigned int width, unsigned int height);

/* Close the kitty image transmission */
void mjx_kitty_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* MJX_KITTY_H */
