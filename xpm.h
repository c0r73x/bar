#ifndef XPM_H_
#define XPM_H_

#include <linux/limits.h>
#include <xcb/xcb_image.h>

typedef struct xpm_icon_t {
    int width, height;
    char filename[PATH_MAX];
    xcb_image_t *image;
} xpm_icon_t;

#define ICON_CACHE_SIZE 256
extern unsigned int icon_count;
extern unsigned int icon_index;
extern xpm_icon_t icon_cache[];

struct xpm_icon_t *load_xpm(xcb_connection_t *conn, char *filename);

#endif // XPM_H_
