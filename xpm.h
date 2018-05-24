#ifndef XPM_H_
#define XPM_H_

#include <xcb/xcb_image.h>

typedef struct xpm_icon_t {
    int width, height;
    char *filename;
    xcb_image_t* image;
} xpm_icon_t;

#define ICON_CACHE_SIZE 256
static int icon_count = 0;
static int icon_index = 0;
static xpm_icon_t* icon_cache[ICON_CACHE_SIZE];

struct xpm_icon_t *load_xpm(xcb_connection_t *conn, char *filename);

#endif // XPM_H_
