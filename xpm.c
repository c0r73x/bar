/* XPM loader, borrowed from imlib2 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "xpm.h"

static FILE *rgb_txt = NULL;
static xpm_icon_t *icon = NULL;

static void xpm_parse_color(char *color, short *r, short *g, short *b)
{
    char buf[4096];

    /* is a #ff00ff like color */
    if (color[0] == '#') {
        int len;
        char val[32];

        len = strlen(color) - 1;

        if (len < 96) {
            int i;

            len /= 3;

            for (i = 0; i < len; i++) {
                val[i] = color[1 + i + (0 * len)];
            }

            val[i] = 0;
            sscanf(val, "%x", r);

            for (i = 0; i < len; i++) {
                val[i] = color[1 + i + (1 * len)];
            }

            val[i] = 0;
            sscanf(val, "%x", g);

            for (i = 0; i < len; i++) {
                val[i] = color[1 + i + (2 * len)];
            }

            val[i] = 0;
            sscanf(val, "%x", b);

            if (len == 1) {
                *r = (*r << 4) | *r;
                *g = (*g << 4) | *g;
                *b = (*b << 4) | *b;
            } else if (len > 2) {
                *r >>= (len - 2) * 4;
                *g >>= (len - 2) * 4;
                *b >>= (len - 2) * 4;
            }
        }

        return;
    }

    /* look in rgb txt database */
    if (!rgb_txt)
    #ifndef __EMX__
        rgb_txt = fopen("/usr/share/X11/rgb.txt", "r");

    if (!rgb_txt) {
        rgb_txt = fopen("/usr/X11R6/lib/X11/rgb.txt", "r");
    }

    if (!rgb_txt) {
        rgb_txt = fopen("/usr/openwin/lib/X11/rgb.txt", "r");
    }

    #else
        rgb_txt = fopen(__XOS2RedirRoot("/XFree86/lib/X11/rgb.txt"), "rt");
    #endif

    if (!rgb_txt) {
        return;
    }

    fseek(rgb_txt, 0, SEEK_SET);

    while (fgets(buf, 4000, rgb_txt)) {
        if (buf[0] != '!') {
            int rr, gg, bb;
            char name[4096];

            sscanf(buf, "%i %i %i %[^\n]", &rr, &gg, &bb, name);

            if (!strcasecmp(name, color)) {
                *r = rr;
                *g = gg;
                *b = bb;
                return;
            }
        }
    }
}

static void xpm_parse_done(void)
{
    if (rgb_txt) {
        fclose(rgb_txt);
    }

    rgb_txt = NULL;

    if (icon != NULL) {
        if(icon->image != NULL) {
            xcb_image_destroy(icon->image);
            icon->image = NULL;
        }

        if(icon->filename != NULL) {
            free(icon->filename);
            icon->filename = NULL;
        }

        free(icon);
        icon = NULL;
    }
}

struct xpm_icon_t *load_xpm(xcb_connection_t *conn, char *filename)
{
    if (filename == NULL || strlen(filename) == 0) {
        return 0;
    }

    for (int i=0; i < icon_count; i++) {
        if (icon_cache[i]->filename != NULL) {
            if (strcmp(icon_cache[i]->filename, filename) == 0) {
                return icon_cache[i];
            }
        }
    }


    struct _cmap {
        unsigned char str[6];
        unsigned char transp;
        short r, g, b;
    } *cmap;

    uint8_t *ptr;
    FILE *f;

    int pc, c, i, j, k, w, h, ncolors, cpp, comment, quote, context, len, done, backslash;
    char *line, s[256], tok[256], col[256];
    int lsz = 256;

    short lookup[128 - 32][128 - 32];
    int count, pixels;

    done = 0;

    f = fopen(filename, "rb");

    if (!f) {
        xpm_parse_done();
        return 0;
    }

    fread(s, 1, 9, f);
    rewind(f);
    s[9] = 0;

    if (strcmp("/* XPM */", s)) {
        fclose(f);
        xpm_parse_done();
        return 0;
    }

    icon = malloc(sizeof(xpm_icon_t));
    icon->filename = malloc(strlen(filename));
    strcpy(icon->filename, filename);

    i = 0;
    j = 0;
    cmap = NULL;
    w = 10;
    h = 10;
    ptr = NULL;
    c = ' ';
    comment = 0;
    quote = 0;
    context = 0;
    pixels = 0;
    count = 0;
    line = malloc(lsz);

    if (!line) {
        fclose(f);
        xpm_parse_done();
        return 0;
    }

    backslash = 0;
    memset(lookup, 0, sizeof(lookup));

    while (!done) {
        pc = c;
        c = fgetc(f);

        if (c == EOF) {
            break;
        }

        if (!quote) {
            if ((pc == '/') && (c == '*')) {
                comment = 1;
            } else if ((pc == '*') && (c == '/') && (comment)) {
                comment = 0;
            }
        }

        if (!comment) {
            if ((!quote) && (c == '"')) {
                quote = 1;
                i = 0;
            } else if ((quote) && (c == '"')) {
                line[i] = 0;
                quote = 0;

                if (context == 0) {
                    /* Header */
                    sscanf(line, "%i %i %i %i", &w, &h, &ncolors, &cpp);

                    if ((ncolors > 32766) || (ncolors < 1)) {
                        fprintf(stderr, "XPM ERROR: XPM files with colors > 32766 or < 1 not supported\n");
                        free(line);
                        fclose(f);
                        xpm_parse_done();
                        return 0;
                    }

                    if ((cpp > 5) || (cpp < 1)) {
                        fprintf(stderr, "XPM ERROR: XPM files with characters per pixel > 5 or < 1not supported\n");
                        free(line);
                        fclose(f);
                        xpm_parse_done();
                        return 0;
                    }

                    icon->width = w;
                    icon->height = h;

                    cmap = malloc(sizeof(struct _cmap) * ncolors);

                    if (!cmap) {
                        free(line);
                        fclose(f);
                        xpm_parse_done();
                        return 0;
                    }

                    xcb_image_t *img = xcb_image_create_native(
                        conn,
                        w,
                        h,
                        XCB_IMAGE_FORMAT_Z_PIXMAP,
                        32,
                        NULL,
                        ~0,
                        NULL
                    );

                    img->data = malloc(img->size);
                    if (img->data == NULL) {
                        free(cmap);
                        free(line);
                        fclose(f);
                        xpm_parse_done();
                        return 0;
                    }

                    icon->image = img;
                    ptr = &img->data[0];
                    pixels = w * h * 4;

                    j = 0;
                    context++;
                } else if (context == 1) {
                    /* Color Table */
                    if (j < ncolors) {
                        int slen;
                        int hascolor, iscolor;

                        iscolor = 0;
                        hascolor = 0;
                        tok[0] = 0;
                        col[0] = 0;
                        s[0] = 0;
                        len = strlen(line);
                        strncpy(cmap[j].str, line, cpp);
                        cmap[j].str[cpp] = 0;
                        cmap[j].r = -1;
                        cmap[j].transp = 0;

                        for (k = cpp; k < len; k++) {
                            if (line[k] != ' ') {
                                s[0] = 0;
                                sscanf(&line[k], "%255s", s);
                                slen = strlen(s);
                                k += slen;

                                if (!strcmp(s, "c")) {
                                    iscolor = 1;
                                }

                                if ((!strcmp(s, "m")) || (!strcmp(s, "s"))
                                    || (!strcmp(s, "g4"))
                                    || (!strcmp(s, "g"))
                                    || (!strcmp(s, "c")) || (k >= len)) {
                                    if (k >= len) {
                                        if (col[0]) {
                                            if (strlen(col) < (sizeof(col) - 2)) {
                                                strcat(col, " ");
                                            } else {
                                                done = 1;
                                            }
                                        }

                                        if (strlen(col) + strlen(s) <
                                            (sizeof(col) - 1)) {
                                            strcat(col, s);
                                        }
                                    }

                                    if (col[0]) {
                                        if (!strcasecmp(col, "none")) {
                                            cmap[j].transp = 1;
                                        } else {
                                            if ((((cmap[j].r < 0) || (!strcmp(tok, "c"))) && (!hascolor))) {
                                                xpm_parse_color(
                                                    col,
                                                    &cmap[j].r,
                                                    &cmap[j].g,
                                                    &cmap[j].b
                                                );

                                                if (iscolor) {
                                                    hascolor = 1;
                                                }
                                            }
                                        }
                                    }

                                    strcpy(tok, s);
                                    col[0] = 0;
                                } else {
                                    if (col[0]) {
                                        if (strlen(col) < (sizeof(col) - 2)) {
                                            strcat(col, " ");
                                        } else {
                                            done = 1;
                                        }
                                    }

                                    if (strlen(col) + strlen(s) <
                                        (sizeof(col) - 1)) {
                                        strcat(col, s);
                                    }
                                }
                            }
                        }
                    }

                    j++;

                    if (j >= ncolors) {
                        if (cpp == 1) {
                            for (i = 0; i < ncolors; i++) {
                                lookup[(int)cmap[i].str[0] - 32][0] = i;
                            }
                        }

                        if (cpp == 2) {
                            for (i = 0; i < ncolors; i++) {
                                lookup[(int)cmap[i].str[0] - 32]
                                    [(int)cmap[i].str[1] - 32] = i;
                            }
                        }

                        context++;
                    }
                } else {
                    /* Image Data */
                    i = 0;

                    if (cpp == 0) {
                        /* Chars per pixel = 0? well u never know */
                    }

                    if (cpp == 1) {
                        for (i = 0; ((i < 65536) && (count < pixels) && (line[i])); i++) {
                            col[0] = line[i];

                            if (cmap[lookup[(int)col[0] - 32][0]].transp) {
                                ptr[count++] = 0x00;
                                ptr[count++] = 0x00;
                                ptr[count++] = 0x00;
                                ptr[count++] = 0x00;
                            } else {
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][0]].b;
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][0]].g;
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][0]].r;
                                ptr[count++] = 0xff;
                            }
                        }
                    } else if (cpp == 2) {
                        for (i = 0; ((i < 65536) && (count < pixels) && (line[i])); i++) {
                            col[0] = line[i++];
                            col[1] = line[i];

                            if (cmap[lookup[(int)col[0] - 32][(int)col[1] - 32]].transp) {
                                ptr[count++] = 0x00;
                                ptr[count++] = 0x00;
                                ptr[count++] = 0x00,
                                ptr[count++] = 0x00;
                            } else {
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][(int)col[1] - 32]].b;
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][(int)col[1] - 32]].g;
                                ptr[count++] = cmap[lookup[(int)col[0] - 32][(int)col[1] - 32]].r;
                                ptr[count++] = 0xff;
                            }
                        }
                    } else {
                        for (i = 0; ((i < 65536) && (count < pixels) && (line[i])); i++) {
                            for (j = 0; j < cpp; j++, i++) {
                                col[j] = line[i];
                            }

                            col[j] = 0;
                            i--;

                            for (j = 0; j < ncolors; j++) {
                                if (!strcmp(col, cmap[j].str)) {
                                    if (cmap[j].transp) {
                                        ptr[count++] = 0x00;
                                        ptr[count++] = 0x00;
                                        ptr[count++] = 0x00;
                                        ptr[count++] = 0x00;
                                    } else {
                                        ptr[count++] = cmap[j].b;
                                        ptr[count++] = cmap[j].g;
                                        ptr[count++] = cmap[j].r;
                                        ptr[count++] = 0xff;
                                    }

                                    j = ncolors;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Scan in line from XPM file */
        if ((!comment) && (quote) && (c != '"')) {
            if (c < 32) {
                c = 32;
            } else if (c > 127) {
                c = 127;
            }

            if (c == '\\') {
                if (++backslash < 2) {
                    line[i++] = c;
                } else {
                    backslash = 0;
                }
            } else {
                backslash = 0;
                line[i++] = c;
            }
        }

        if (i >= lsz) {
            lsz += 256;
            line = realloc(line, lsz);
        }

        if (((context > 1) && (count >= pixels))) {
            done = 1;
        }
    }

    fclose(f);
    free(cmap);
    free(line);

    if (rgb_txt) {
        fclose(rgb_txt);
    }

    rgb_txt = NULL;

    if (icon_count < ICON_CACHE_SIZE) {
        icon_count++;
    }

    icon_cache[icon_index] = icon;
    icon_index++;
    icon_index %= ICON_CACHE_SIZE;

    return icon;
}
