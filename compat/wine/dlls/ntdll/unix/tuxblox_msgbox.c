/*
 * TuxBlox hard error message box
 *
 * A program reporting a hard error is telling the user something in its own
 * words, and Windows draws that message itself. This side cannot call user32,
 * so it draws the box on the host display instead, from a forked child, and
 * reports which button was pressed.
 *
 * Copyright 2026 TuxBlox Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "unix_private.h"

#if defined(__has_include)
#if __has_include(<png.h>)
#include <png.h>
#define MSGBOX_HAVE_PNG
#endif
#endif

/* X11 last: its headers define Status, ControlMask and other names the Windows
 * ones use as ordinary identifiers. */
#ifdef SONAME_LIBX11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#endif

/* The running program's file name, without its path or extension. */
BOOL tuxblox_program_name( char *buffer, size_t size )
{
    static const WCHAR dot_exe[] = { '.','e','x','e' };
    const UNICODE_STRING *path = &NtCurrentTeb()->Peb->ProcessParameters->ImagePathName;
    const WCHAR *name = path->Buffer;
    size_t len = path->Length / sizeof(WCHAR), i;

    if (!name) return FALSE;

    for (i = len; i > 0; i--) if (name[i - 1] == '\\' || name[i - 1] == '/') break;
    name += i;
    len -= i;
    if (len > 4 && !wcsnicmp( name + len - 4, dot_exe, 4 )) len -= 4;
    if (!len || len >= size) return FALSE;
    if (!ntdll_wcstoumbs( name, len, buffer, size - 1, FALSE )) return FALSE;
    buffer[min( len, size - 1 )] = 0;
    return TRUE;
}


/* What the child reports back. Anything else means it never got a window up
 * and the caller still has to find another way to show the message. */
/* The same flags MessageBox takes: the low nibble picks the buttons, the next
 * one picks the icon, and 0x100 makes the second button the one Enter presses. */
#define MSGBOX_BUTTONS          0x0000000f
#define MSGBOX_OK               0x00000000
#define MSGBOX_OKCANCEL         0x00000001
#define MSGBOX_ABORTRETRYIGNORE 0x00000002
#define MSGBOX_YESNOCANCEL      0x00000003
#define MSGBOX_YESNO            0x00000004
#define MSGBOX_RETRYCANCEL      0x00000005
#define MSGBOX_CANCELTRYCONTINUE 0x00000006
#define MSGBOX_ICON             0x000000f0
#define MSGBOX_ICON_ERROR       0x00000010
#define MSGBOX_ICON_QUESTION    0x00000020
#define MSGBOX_ICON_WARNING     0x00000030
#define MSGBOX_ICON_INFO        0x00000040
#define MSGBOX_DEFBUTTON        0x00000f00
#define MSGBOX_DEFBUTTON2       0x00000100

#define MSGBOX_NOT_SHOWN        (-1)
#define MSGBOX_DISMISSED        (-2)
/* The same two, as a drawn box's child has to report them through an exit code. */
#define MSGBOX_EXIT_DISMISSED   8
#define MSGBOX_EXIT_NOT_SHOWN   9
#define MSGBOX_MAX_BUTTONS      3

/* What a message box offers and what each answer is called back to the caller.
 * The order is the order they are read in, which puts the one that carries on
 * last, as every other TuxBlox window does -- Windows reads them the other way
 * round, and nothing outside this table depends on which way that is. */
struct msgbox_choice
{
    const char *label;
    unsigned int response;
};

struct msgbox_choices
{
    struct msgbox_choice choice[MSGBOX_MAX_BUTTONS];
    int count;
    int preferred;      /* pressed by Enter unless the caller names another */
    BOOL can_dismiss;   /* false where Windows would grey out the window's close button */
};

/* Chooses the set from the same flags MessageBox takes. */
static void msgbox_set( unsigned int flags, struct msgbox_choices *set )
{
    static const struct msgbox_choices sets[] =
    {
        { { { "OK", ResponseOk } }, 1, 0, TRUE },
        { { { "Cancel", ResponseCancel }, { "OK", ResponseOk } }, 2, 1, TRUE },
        { { { "Abort", ResponseAbort }, { "Ignore", ResponseIgnore }, { "Retry", ResponseRetry } }, 3, 2, FALSE },
        { { { "Cancel", ResponseCancel }, { "No", ResponseNo }, { "Yes", ResponseYes } }, 3, 2, TRUE },
        { { { "No", ResponseNo }, { "Yes", ResponseYes } }, 2, 1, FALSE },
        { { { "Cancel", ResponseCancel }, { "Retry", ResponseRetry } }, 2, 1, TRUE },
        { { { "Cancel", ResponseCancel }, { "Continue", ResponseIgnore }, { "Try Again", ResponseRetry } }, 3, 2, TRUE },
    };
    unsigned int which = flags & MSGBOX_BUTTONS;
    unsigned int chosen;

    if (which >= ARRAY_SIZE(sets)) which = MSGBOX_OK;
    *set = sets[which];

    /* MB_DEFBUTTON2 and its neighbours count from the left as Windows reads
     * them, so they are counted back from the right as this table reads them. */
    chosen = (flags & MSGBOX_DEFBUTTON) >> 8;
    if (chosen < (unsigned int)set->count) set->preferred = set->count - 1 - chosen;
}

/* Where the launcher writes its answer, and where it reads the window's icon, each as a number and as the argument that says so. */
#define MSGBOX_ANSWER_FD        3
#define MSGBOX_ANSWER_FD_ARG    "3"
#define MSGBOX_ICON_FD          4
#define MSGBOX_ICON_FD_ARG      "4"


/* The program's own icon, which is PE resources and a PNG decoder and has
 * nothing to do with how the box is drawn -- the launcher wants it as much as
 * the drawn box does, and a machine with no X11 on it can still have both. */
#ifdef MSGBOX_HAVE_PNG
static void *png_handle;
static int (*p_png_image_begin_read_from_memory)( png_imagep, const void *, size_t );
static int (*p_png_image_finish_read)( png_imagep, png_const_colorp, void *, png_int_32, void * );
static void (*p_png_image_free)( png_imagep );
#endif

/* The size an icon is picked and scaled for, and the largest one worth reading. */
#define MSGBOX_ICON_WANTED       48
#define MSGBOX_ICON_MAX         512

/* Loaded here and not in the child: dlopen takes the loader's lock, and a
 * process that forked while another thread held it would never get it back. */
static void load_png(void)
{
#ifdef MSGBOX_HAVE_PNG
    /* Modern programs, Roblox among them, keep their icons as PNG rather than
     * as the bitmap an icon used to be. */
    if (png_handle || !(png_handle = dlopen( "libpng16.so.16", RTLD_NOW ))) return;

    p_png_image_begin_read_from_memory = dlsym( png_handle, "png_image_begin_read_from_memory" );
    p_png_image_finish_read = dlsym( png_handle, "png_image_finish_read" );
    p_png_image_free = dlsym( png_handle, "png_image_free" );

    if (!p_png_image_begin_read_from_memory || !p_png_image_finish_read || !p_png_image_free)
        p_png_image_begin_read_from_memory = NULL;
#endif
}

/*
 * The icon the window manager shows for the box is the one belonging to the
 * program that raised the error, read out of the copy of it already mapped
 * into this process. When there is none, no icon is set and the window manager
 * falls back on the class hint, which is TuxBlox's own.
 */

/* BITMAPINFOHEADER, as an icon stores it. */
struct icon_header
{
    DWORD size;
    LONG width, height;
    WORD planes, bpp;
    DWORD compression, image_size;
    LONG x_ppm, y_ppm;
    DWORD used, important;
};

#include "pshpack1.h"
struct icon_group_entry
{
    BYTE width, height, colors, reserved;
    WORD planes, bpp;
    DWORD size;
    WORD id;
};
#include "poppack.h"

struct resources
{
    const BYTE *base;
    const IMAGE_RESOURCE_DIRECTORY *root;
    DWORD size;
};

static BOOL find_resources( struct resources *res )
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)NtCurrentTeb()->Peb->ImageBaseAddress;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_DATA_DIRECTORY *dir;

    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    nt = (const IMAGE_NT_HEADERS32 *)((const BYTE *)dos + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        if (nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) return FALSE;
        dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    }
    else
    {
        const IMAGE_NT_HEADERS64 *nt64 = (const IMAGE_NT_HEADERS64 *)nt;

        if (nt64->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return FALSE;
        if (nt64->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) return FALSE;
        dir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    }
    if (!dir->VirtualAddress || dir->Size < sizeof(IMAGE_RESOURCE_DIRECTORY)) return FALSE;

    res->base = (const BYTE *)dos;
    res->root = (const IMAGE_RESOURCE_DIRECTORY *)(res->base + dir->VirtualAddress);
    res->size = dir->Size;
    return TRUE;
}

static BOOL resource_holds( const struct resources *res, const void *ptr, size_t len )
{
    const BYTE *start = (const BYTE *)res->root;

    return (const BYTE *)ptr >= start && (const BYTE *)ptr + len <= start + res->size;
}

/* Walks the three levels a resource directory always has: type, then name,
 * then language. A name below -1 takes whichever comes first. */
static const void *find_resource( const struct resources *res, WORD type, int name, DWORD *size )
{
    const IMAGE_RESOURCE_DIRECTORY *dir = res->root;
    const IMAGE_RESOURCE_DATA_ENTRY *data;
    int level;

    for (level = 0; level < 3; level++)
    {
        const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
        const int count = dir->NumberOfNamedEntries + dir->NumberOfIdEntries;
        const int wanted = level == 0 ? type : (level == 1 ? name : -1);
        int i;

        if (!resource_holds( res, dir, sizeof(*dir) )) return NULL;
        if (!resource_holds( res, entry, count * sizeof(*entry) )) return NULL;

        for (i = 0; i < count; i++)
        {
            if (wanted >= 0 && (entry[i].NameIsString || entry[i].Id != wanted)) continue;
            if (level < 2)
            {
                if (!entry[i].DataIsDirectory) return NULL;
                dir = (const IMAGE_RESOURCE_DIRECTORY *)((const BYTE *)res->root + entry[i].OffsetToDirectory);
                break;
            }
            if (entry[i].DataIsDirectory) return NULL;
            data = (const IMAGE_RESOURCE_DATA_ENTRY *)((const BYTE *)res->root + entry[i].OffsetToData);
            if (!resource_holds( res, data, sizeof(*data) )) return NULL;
            *size = data->Size;
            return res->base + data->OffsetToData;
        }
        if (i == count) return NULL;
    }
    return NULL;
}

/* The one closest to the size a title bar wants, out of those not tried yet. */
static int pick_icon( const struct icon_group_entry *entries, int count, unsigned int tried )
{
    int best = -1, best_size = 0, i;

    for (i = 0; i < count && i < 32; i++)
    {
        const int size = entries[i].width ? entries[i].width : 256;

        if (tried & (1u << i)) continue;
        if (best < 0 || abs( size - MSGBOX_ICON_WANTED ) < abs( best_size - MSGBOX_ICON_WANTED ))
        {
            best = i;
            best_size = size;
        }
    }
    return best;
}

/* Both kinds of icon end up as rows of blue, green, red and alpha. */
static unsigned char *decode_png_icon( const void *data, DWORD size, int *width, int *height )
{
#ifdef MSGBOX_HAVE_PNG
    unsigned char *pixels;
    png_image image;

    if (!p_png_image_begin_read_from_memory) return NULL;

    memset( &image, 0, sizeof(image) );
    image.version = PNG_IMAGE_VERSION;
    if (!p_png_image_begin_read_from_memory( &image, data, size )) return NULL;

    image.format = PNG_FORMAT_BGRA;
    if (image.width > MSGBOX_ICON_MAX || image.height > MSGBOX_ICON_MAX || !image.width ||
        !image.height || !(pixels = malloc( PNG_IMAGE_SIZE( image ) )))
    {
        p_png_image_free( &image );
        return NULL;
    }
    if (!p_png_image_finish_read( &image, NULL, pixels, 0, NULL ))
    {
        p_png_image_free( &image );
        free( pixels );
        return NULL;
    }
    *width = image.width;
    *height = image.height;
    return pixels;
#else
    return NULL;
#endif
}

/* A plain box filter, which is all an icon on its way down to title bar size
 * needs, and keeps the property the window manager is handed small. */
static unsigned char *scale_icon( unsigned char *pixels, int *width, int *height )
{
    const int source_width = *width, source_height = *height;
    unsigned char *scaled;
    int x, y, channel;

    if (source_width <= MSGBOX_ICON_WANTED && source_height <= MSGBOX_ICON_WANTED) return pixels;
    if (!(scaled = malloc( (size_t)MSGBOX_ICON_WANTED * MSGBOX_ICON_WANTED * 4 ))) return pixels;

    for (y = 0; y < MSGBOX_ICON_WANTED; y++)
    {
        const int top = y * source_height / MSGBOX_ICON_WANTED;
        const int bottom = (y + 1) * source_height / MSGBOX_ICON_WANTED;

        for (x = 0; x < MSGBOX_ICON_WANTED; x++)
        {
            const int left = x * source_width / MSGBOX_ICON_WANTED;
            const int right = (x + 1) * source_width / MSGBOX_ICON_WANTED;

            for (channel = 0; channel < 4; channel++)
            {
                int total = 0, count = 0, sx, sy;

                for (sy = top; sy < bottom || sy == top; sy++)
                {
                    for (sx = left; sx < right || sx == left; sx++)
                    {
                        total += pixels[((size_t)sy * source_width + sx) * 4 + channel];
                        count++;
                    }
                }
                scaled[((size_t)y * MSGBOX_ICON_WANTED + x) * 4 + channel] = total / count;
            }
        }
    }

    free( pixels );
    *width = *height = MSGBOX_ICON_WANTED;
    return scaled;
}

static unsigned char *load_icon_pixels( const struct resources *res, WORD id, int *width,
                                        int *height, BOOL *bottom_up )
{
    static const unsigned char png_signature[] = { 0x89, 'P', 'N', 'G' };
    const struct icon_header *icon;
    unsigned char *pixels;
    DWORD size = 0;

    if (!(icon = find_resource( res, 3, id, &size ))) return NULL;

    if (size > sizeof(png_signature) && !memcmp( icon, png_signature, sizeof(png_signature) ))
    {
        *bottom_up = FALSE;
        return decode_png_icon( icon, size, width, height );
    }

    if (size < sizeof(*icon) || icon->size < sizeof(*icon) || icon->bpp != 32) return NULL;
    *width = icon->width;
    *height = icon->height / 2;
    if (*width <= 0 || *height <= 0 || *width > MSGBOX_ICON_MAX || *height > MSGBOX_ICON_MAX) return NULL;
    if (size < icon->size + (DWORD)*width * *height * 4) return NULL;

    if (!(pixels = malloc( (size_t)*width * *height * 4 ))) return NULL;
    memcpy( pixels, (const unsigned char *)icon + icon->size, (size_t)*width * *height * 4 );
    *bottom_up = TRUE;
    return pixels;
}

/* The program's best icon, as the launcher wants it: width, height, then rows
 * of red, green, blue and alpha from the top down. Handed over as an open file
 * that has already been unlinked, so there is nothing left behind to tidy up
 * if this process is killed while the box is still on screen.
 * Returns -1 when the program carries no icon this can read. */
static int open_program_icon(void)
{
    const struct icon_group_entry *entries;
    char path[PATH_MAX];
    unsigned char *pixels = NULL;
    BOOL bottom_up = FALSE;
    struct resources res;
    unsigned int tried = 0;
    const void *group;
    DWORD group_size = 0;
    int count, chosen, width = 0, height = 0, y;
    unsigned char header[8];
    int fd;

    /* Needed before any of this can be read: a modern program keeps its icon as a PNG. */
    load_png();

    if (!find_resources( &res )) return -1;
    if (!(group = find_resource( &res, 14, -1, &group_size )) || group_size < 6 + sizeof(*entries))
        return -1;

    count = ((const WORD *)group)[2];
    entries = (const struct icon_group_entry *)((const BYTE *)group + 6);
    if (group_size < 6 + count * sizeof(*entries)) return -1;

    /* The best size first, then the next best if that one cannot be read. */
    while (!pixels && (chosen = pick_icon( entries, count, tried )) >= 0)
    {
        tried |= 1u << chosen;
        pixels = load_icon_pixels( &res, entries[chosen].id, &width, &height, &bottom_up );
    }
    if (!pixels) return -1;

    if (snprintf( path, sizeof(path), "%s/tuxblox-icon-XXXXXX",
                  getenv( "XDG_RUNTIME_DIR" ) ? getenv( "XDG_RUNTIME_DIR" ) : "/tmp" ) >= (int)sizeof(path) ||
        (fd = mkstemp( path )) == -1)
    {
        free( pixels );
        return -1;
    }
    unlink( path );

    header[0] = width; header[1] = width >> 8; header[2] = width >> 16; header[3] = width >> 24;
    header[4] = height; header[5] = height >> 8; header[6] = height >> 16; header[7] = height >> 24;
    if (write( fd, header, sizeof(header) ) != sizeof(header)) height = 0;

    /* Blue and red the other way round from how an icon stores them, and the
     * rows the other way up when it came from a bitmap rather than a PNG. */
    for (y = 0; y < height; y++)
    {
        unsigned char *row = pixels + (size_t)(bottom_up ? height - 1 - y : y) * width * 4;
        int x;

        for (x = 0; x < width; x++)
        {
            const unsigned char blue = row[x * 4];

            row[x * 4] = row[x * 4 + 2];
            row[x * 4 + 2] = blue;
        }
        if (write( fd, row, (size_t)width * 4 ) != (ssize_t)width * 4) break;
    }

    free( pixels );
    if (y == height && height > 0 && lseek( fd, 0, SEEK_SET ) == 0) return fd;
    close( fd );
    return -1;
}

#ifdef SONAME_LIBX11

/* The same metrics user32 lays a message box out with, so the two look alike. */
/* The installer's window and the launcher's popups, measured off them, so a
 * message from the compatibility layer looks like the rest of TuxBlox rather
 * than like a second program. 24 + 72 + 20 + 380 + 24 is the 520 they use. */
#define MSGBOX_MARGIN            24
#define MSGBOX_ICON_SIZE         32
#define MSGBOX_LOGO_SIZE         72
#define MSGBOX_ICON_GAP          20
#define MSGBOX_HEADING_GAP        6
#define MSGBOX_CONTENT_GAP       16
#define MSGBOX_BUTTON_GAP         8
#define MSGBOX_BUTTON_HEIGHT     34
#define MSGBOX_BUTTON_MIN_WIDTH  80
#define MSGBOX_BUTTON_PADDING    16
#define MSGBOX_BUTTON_RADIUS      6
#define MSGBOX_BUTTON_EDGE       18
#define MSGBOX_BUTTON_BOTTOM     14
#define MSGBOX_SUPERSAMPLE        4
#define MSGBOX_TEXT_MAX_WIDTH   380
#define MSGBOX_MAX_LINES         64

/* Xft is declared here rather than included: ntdll builds against X11 alone,
 * and these few types have been fixed for as long as the library has existed. */
typedef struct { unsigned short red, green, blue, alpha; } msgbox_render_color;
typedef struct { unsigned long pixel; msgbox_render_color color; } msgbox_xft_color;
typedef struct { int ascent, descent, height, max_advance_width; } msgbox_xft_font;
typedef struct { unsigned short width, height; short x, y; short x_off, y_off; } msgbox_glyph_info;

struct msgbox_colors
{
    unsigned int back;
    unsigned int text;
    unsigned int button_text;
    unsigned int accent;
};

struct msgbox_line
{
    const char *text;
    int len;
};

struct msgbox_button
{
    const char *label;
    int x, width;
    BOOL affirmative;
};

struct msgbox
{
    Display *display;
    Window window;
    Pixmap canvas;
    GC gc;
    int depth;
    Colormap colormap;
    Visual *visual;
    int screen;
    int width, height;
    struct msgbox_colors colors;
    msgbox_xft_font *font;
    msgbox_xft_font *body_font;
    msgbox_xft_font *bold_font;
    const char *heading;
    int heading_len;
    int icon_size;
    void *xft_draw;
    XFontStruct *core_font;
    struct msgbox_line lines[MSGBOX_MAX_LINES];
    int line_count;
    struct msgbox_button buttons[MSGBOX_MAX_BUTTONS];
    int button_count;
    unsigned int flags;
    int focused;
    int hot;
    int pressed;
    Atom delete_window;
};

static void *x11_handle;
static void *xft_handle;

#define MSGBOX_FUNC(f) static typeof(f) *p_##f
MSGBOX_FUNC(XAllocColor);
MSGBOX_FUNC(XChangeProperty);
MSGBOX_FUNC(XCloseDisplay);
MSGBOX_FUNC(XConnectionNumber);
MSGBOX_FUNC(XPending);
MSGBOX_FUNC(XCopyArea);
MSGBOX_FUNC(XCreateGC);
MSGBOX_FUNC(XCreateImage);
MSGBOX_FUNC(XCreatePixmap);
MSGBOX_FUNC(XCreateSimpleWindow);
MSGBOX_FUNC(XDefaultColormap);
MSGBOX_FUNC(XDefaultDepth);
MSGBOX_FUNC(XDefaultScreen);
MSGBOX_FUNC(XDefaultVisual);
MSGBOX_FUNC(XDisplayHeight);
MSGBOX_FUNC(XDisplayWidth);
MSGBOX_FUNC(XDrawLine);
MSGBOX_FUNC(XDrawString);
MSGBOX_FUNC(XFillArc);
MSGBOX_FUNC(XFillRectangle);
MSGBOX_FUNC(XFlush);
MSGBOX_FUNC(XFreeGC);
MSGBOX_FUNC(XFreePixmap);
MSGBOX_FUNC(XInternAtom);
MSGBOX_FUNC(XLoadQueryFont);
MSGBOX_FUNC(XLookupKeysym);
MSGBOX_FUNC(XMapRaised);
MSGBOX_FUNC(XPutImage);
MSGBOX_FUNC(XNextEvent);
MSGBOX_FUNC(XOpenDisplay);
MSGBOX_FUNC(XRefreshKeyboardMapping);
MSGBOX_FUNC(XRootWindow);
MSGBOX_FUNC(XSelectInput);
MSGBOX_FUNC(XSetClassHint);
MSGBOX_FUNC(XSetFont);
MSGBOX_FUNC(XSetForeground);
MSGBOX_FUNC(XSetLineAttributes);
MSGBOX_FUNC(XSetWMNormalHints);
MSGBOX_FUNC(XSetWMProtocols);
MSGBOX_FUNC(XStoreName);
MSGBOX_FUNC(XTextWidth);
#undef MSGBOX_FUNC

static msgbox_xft_font *(*p_XftFontOpenName)( Display *, int, const char * );
static void *(*p_XftDrawCreate)( Display *, Drawable, Visual *, Colormap );
static void (*p_XftDrawDestroy)( void * );
static void (*p_XftDrawStringUtf8)( void *, const msgbox_xft_color *, msgbox_xft_font *,
                                    int, int, const unsigned char *, int );
static void (*p_XftTextExtentsUtf8)( Display *, msgbox_xft_font *, const unsigned char *,
                                     int, msgbox_glyph_info * );
static int (*p_XftColorAllocValue)( Display *, Visual *, Colormap,
                                    const msgbox_render_color *, msgbox_xft_color * );
static void (*p_XftColorFree)( Display *, Visual *, Colormap, msgbox_xft_color * );

/* Loaded here and not in the child: dlopen takes the loader's lock, and a
 * process that forked while another thread held it would never get it back. */
static BOOL load_libraries(void)
{
    if (x11_handle) return TRUE;
    if (!(x11_handle = dlopen( SONAME_LIBX11, RTLD_NOW ))) return FALSE;

#define MSGBOX_FUNC(f) \
    if (!(p_##f = dlsym( x11_handle, #f ))) return FALSE
    MSGBOX_FUNC(XAllocColor);
    MSGBOX_FUNC(XChangeProperty);
    MSGBOX_FUNC(XCloseDisplay);
    MSGBOX_FUNC(XConnectionNumber);
    MSGBOX_FUNC(XPending);
    MSGBOX_FUNC(XCopyArea);
    MSGBOX_FUNC(XCreateGC);
    MSGBOX_FUNC(XCreateImage);
    MSGBOX_FUNC(XCreatePixmap);
    MSGBOX_FUNC(XCreateSimpleWindow);
    MSGBOX_FUNC(XDefaultColormap);
    MSGBOX_FUNC(XDefaultDepth);
    MSGBOX_FUNC(XDefaultScreen);
    MSGBOX_FUNC(XDefaultVisual);
    MSGBOX_FUNC(XDisplayHeight);
    MSGBOX_FUNC(XDisplayWidth);
    MSGBOX_FUNC(XDrawLine);
    MSGBOX_FUNC(XDrawString);
    MSGBOX_FUNC(XFillArc);
    MSGBOX_FUNC(XFillRectangle);
    MSGBOX_FUNC(XFlush);
    MSGBOX_FUNC(XFreeGC);
    MSGBOX_FUNC(XFreePixmap);
    MSGBOX_FUNC(XInternAtom);
    MSGBOX_FUNC(XLoadQueryFont);
    MSGBOX_FUNC(XLookupKeysym);
    MSGBOX_FUNC(XMapRaised);
    MSGBOX_FUNC(XPutImage);
    MSGBOX_FUNC(XNextEvent);
    MSGBOX_FUNC(XOpenDisplay);
    MSGBOX_FUNC(XRefreshKeyboardMapping);
    MSGBOX_FUNC(XRootWindow);
    MSGBOX_FUNC(XSelectInput);
    MSGBOX_FUNC(XSetClassHint);
    MSGBOX_FUNC(XSetFont);
    MSGBOX_FUNC(XSetForeground);
    MSGBOX_FUNC(XSetLineAttributes);
    MSGBOX_FUNC(XSetWMNormalHints);
    MSGBOX_FUNC(XSetWMProtocols);
    MSGBOX_FUNC(XStoreName);
    MSGBOX_FUNC(XTextWidth);
#undef MSGBOX_FUNC

    /* Without it the box still opens, drawn in whatever font the X server has. */
    if ((xft_handle = dlopen( "libXft.so.2", RTLD_NOW )))
    {
        p_XftFontOpenName = dlsym( xft_handle, "XftFontOpenName" );
        p_XftDrawCreate = dlsym( xft_handle, "XftDrawCreate" );
        p_XftDrawDestroy = dlsym( xft_handle, "XftDrawDestroy" );
        p_XftDrawStringUtf8 = dlsym( xft_handle, "XftDrawStringUtf8" );
        p_XftTextExtentsUtf8 = dlsym( xft_handle, "XftTextExtentsUtf8" );
        p_XftColorAllocValue = dlsym( xft_handle, "XftColorAllocValue" );
        p_XftColorFree = dlsym( xft_handle, "XftColorFree" );

        if (!p_XftFontOpenName || !p_XftDrawCreate || !p_XftDrawDestroy || !p_XftDrawStringUtf8 ||
            !p_XftTextExtentsUtf8 || !p_XftColorAllocValue || !p_XftColorFree)
            p_XftFontOpenName = NULL;
    }

    load_png();
    return TRUE;
}


/*
 * The look comes out of the prefix, which is where user32 reads it too: the
 * colours the user's desktop was matched to, and the font it was matched to.
 */

static BOOL open_user_reg( FILE **file )
{
    char *path;

    if (!config_dir) return FALSE;
    if (!(path = malloc( strlen(config_dir) + sizeof("/user.reg") ))) return FALSE;

    strcpy( path, config_dir );
    strcat( path, "/user.reg" );
    *file = fopen( path, "r" );
    free( path );
    return *file != NULL;
}

/* Registry sections are written one to a line, so a line starting with '[' is
 * the end of the one being read. */
static BOOL find_reg_section( FILE *file, const char *section, char *line, int size )
{
    while (fgets( line, size, file ))
        if (!strncmp( line, section, strlen(section) )) return TRUE;
    return FALSE;
}

/* The text after "Name"=, or NULL when the line names a different value. */
static const char *reg_value( const char *line, const char *name )
{
    const size_t len = strlen( name );

    if (line[0] != '"' || strncmp( line + 1, name, len ) || strncmp( line + 1 + len, "\"=", 2 )) return NULL;
    return line + len + 3;
}

static unsigned int parse_color( const char *value, unsigned int fallback )
{
    unsigned int red, green, blue;

    if (sscanf( value, " \"%u %u %u\"", &red, &green, &blue ) != 3) return fallback;
    return (red << 16) | (green << 8) | blue;
}

static BOOL is_dark_color( unsigned int color )
{
    return (((color >> 16) & 0xff) * 30 + ((color >> 8) & 0xff) * 59 + (color & 0xff) * 11) / 100 < 128;
}

/* Positive lightens towards white, negative darkens towards black. */
static unsigned int shade_color( unsigned int color, int percent )
{
    int channel[3] = { (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff };
    int i;

    for (i = 0; i < 3; i++)
    {
        if (percent > 0) channel[i] += (255 - channel[i]) * percent / 100;
        else channel[i] += channel[i] * percent / 100;
    }
    return (channel[0] << 16) | (channel[1] << 8) | channel[2];
}

static void read_colors( struct msgbox_colors *colors )
{
    char line[512];
    FILE *file;

    colors->back = 0xd4d0c8;
    colors->text = 0x000000;
    colors->button_text = 0x000000;
    colors->accent = 0x316ac5;

    if (!open_user_reg( &file )) return;
    if (find_reg_section( file, "[Control Panel\\\\Colors]", line, sizeof(line) ))
    {
        while (fgets( line, sizeof(line), file ) && line[0] != '[')
        {
            const char *value;

            if ((value = reg_value( line, "ButtonFace" ))) colors->back = parse_color( value, colors->back );
            else if ((value = reg_value( line, "ButtonText" ))) colors->button_text = parse_color( value, colors->button_text );
            else if ((value = reg_value( line, "WindowText" ))) colors->text = parse_color( value, colors->text );
            else if ((value = reg_value( line, "Hilight" ))) colors->accent = parse_color( value, colors->accent );
        }
    }
    fclose( file );
}

/* A LOGFONTW as the registry stores it: the height is the first field and the
 * face name starts at offset 28, one byte of every pair carrying the letter. */
static void read_font_name( char *name, size_t size )
{
    unsigned char font[92];
    char line[1024], face[32];
    unsigned int count = 0, i;
    const char *p;
    int height;
    FILE *file;

    snprintf( name, size, "Sans:pixelsize=13" );
    if (!open_user_reg( &file )) return;
    if (!find_reg_section( file, "[Control Panel\\\\Desktop\\\\WindowMetrics]", line, sizeof(line) ))
    {
        fclose( file );
        return;
    }

    while (fgets( line, sizeof(line), file ) && line[0] != '[')
    {
        if (!(p = reg_value( line, "MessageFont" )) || strncmp( p, "hex:", 4 )) continue;

        p += 4;
        for (;;)
        {
            while (*p && count < sizeof(font))
            {
                if (isxdigit( (unsigned char)p[0] ) && isxdigit( (unsigned char)p[1] ))
                {
                    font[count++] = (unsigned char)strtoul( (char[3]){ p[0], p[1], 0 }, NULL, 16 );
                    p += 2;
                }
                else if (*p == ',' || *p == ' ') p++;
                else break;
            }
            /* A long value is wrapped over several lines, each but the last
             * ending in a backslash. */
            if (*p != '\\' || count >= sizeof(font)) break;
            if (!fgets( line, sizeof(line), file )) break;
            p = line;
            while (*p == ' ' || *p == '\t') p++;
        }
        break;
    }
    fclose( file );

    if (count < 30) return;

    height = (int)(font[0] | (font[1] << 8) | (font[2] << 16) | ((unsigned int)font[3] << 24));
    if (height < 0) height = -height;
    else if (height > 0) height = height * 4 / 5;
    if (height < 8 || height > 72) height = 13;

    for (i = 0; i + 1 < sizeof(face) && 28 + i * 2 + 1 < count; i++)
    {
        if (!font[28 + i * 2] && !font[29 + i * 2]) break;
        face[i] = (char)font[28 + i * 2];
    }
    face[i] = 0;
    snprintf( name, size, "%s:pixelsize=%d", i ? face : "Sans", height );
}


/*
 * Drawing. Everything below here runs in the child.
 */

static unsigned long mask_channel( unsigned long mask, unsigned int value )
{
    unsigned long shift = 0;

    if (!mask) return 0;
    while (!(mask & 1)) { mask >>= 1; shift++; }
    return ((value * mask) / 255) << shift;
}

static unsigned long x_pixel( struct msgbox *box, unsigned int color )
{
    XColor value;

    if (box->visual->red_mask)
        return mask_channel( box->visual->red_mask, (color >> 16) & 0xff ) |
               mask_channel( box->visual->green_mask, (color >> 8) & 0xff ) |
               mask_channel( box->visual->blue_mask, color & 0xff );

    memset( &value, 0, sizeof(value) );
    value.red = ((color >> 16) & 0xff) * 257;
    value.green = ((color >> 8) & 0xff) * 257;
    value.blue = (color & 0xff) * 257;
    if (!p_XAllocColor( box->display, box->colormap, &value )) return 0;
    return value.pixel;
}

static void fill_rect( struct msgbox *box, unsigned int color, int x, int y, int width, int height )
{
    p_XSetForeground( box->display, box->gc, x_pixel( box, color ) );
    p_XFillRectangle( box->display, box->canvas, box->gc, x, y, width, height );
}

static int text_width( struct msgbox *box, const char *text, int len )
{
    msgbox_glyph_info extents;

    if (box->font)
    {
        p_XftTextExtentsUtf8( box->display, box->font, (const unsigned char *)text, len, &extents );
        return extents.x_off;
    }
    if (box->core_font) return p_XTextWidth( box->core_font, text, len );
    return len * 7;
}

static int text_height( struct msgbox *box )
{
    if (box->font) return box->font->height;
    if (box->core_font) return box->core_font->ascent + box->core_font->descent;
    return 16;
}

static int text_ascent( struct msgbox *box )
{
    if (box->font) return box->font->ascent;
    if (box->core_font) return box->core_font->ascent;
    return 12;
}

static void draw_text( struct msgbox *box, int x, int y, const char *text, int len, unsigned int color )
{
    if (box->font)
    {
        msgbox_render_color render;
        msgbox_xft_color allocated;

        render.red = ((color >> 16) & 0xff) * 257;
        render.green = ((color >> 8) & 0xff) * 257;
        render.blue = (color & 0xff) * 257;
        render.alpha = 0xffff;
        if (!p_XftColorAllocValue( box->display, box->visual, box->colormap, &render, &allocated )) return;
        p_XftDrawStringUtf8( box->xft_draw, &allocated, box->font, x, y + text_ascent( box ),
                             (const unsigned char *)text, len );
        p_XftColorFree( box->display, box->visual, box->colormap, &allocated );
        return;
    }
    if (!box->core_font) return;
    p_XSetForeground( box->display, box->gc, x_pixel( box, color ) );
    p_XDrawString( box->display, box->canvas, box->gc, x, y + text_ascent( box ), text, len );
}

/* Wraps on spaces, and on any the message asks for itself. A word longer than
 * the column is left to run over rather than broken in the middle. */
static int wrap_text( struct msgbox *box, const char *text, int max_width )
{
    int start = 0, last_space = -1, i = 0;
    int count = 0;

    while (text[i] && count < MSGBOX_MAX_LINES)
    {
        if (text[i] == '\n' || text[i] == '\r')
        {
            box->lines[count].text = text + start;
            box->lines[count++].len = i - start;
            if (text[i] == '\r' && text[i + 1] == '\n') i++;
            start = ++i;
            last_space = -1;
            continue;
        }
        if (text[i] == ' ') last_space = i;

        if (text_width( box, text + start, i - start + 1 ) > max_width && last_space > start)
        {
            box->lines[count].text = text + start;
            box->lines[count++].len = last_space - start;
            start = last_space + 1;
            i = start;
            last_space = -1;
            continue;
        }
        i++;
    }
    if (count < MSGBOX_MAX_LINES && i > start)
    {
        box->lines[count].text = text + start;
        box->lines[count++].len = i - start;
    }
    return count;
}

/* The four Windows draws, in the colours a current desktop uses for them: a
 * cross for an error, an exclamation for a warning, and a letter for the two
 * that are only telling you something. */
/* The drawing helpers take their face from box->font, so the heading swaps the
 * bold one in and puts the body one back. */
static void use_font( struct msgbox *box, msgbox_xft_font *font )
{
    if (font) box->font = font;
}

static unsigned char *load_tuxblox_icon( int *width, int *height );

/* The logo at the size the box draws it, scaled once and kept: a box repaints
 * on every expose, and the source is a 256-pixel image. Averaging the source
 * pixels that fall inside each destination one is what keeps it from looking
 * ragged the way picking a single pixel would. */
static const unsigned char *logo_pixels( int size )
{
    static unsigned char *cached;
    static int cached_size;
    unsigned char *source, *scaled;
    int width = 0, height = 0, x, y;

    if (cached_size == size) return cached;
    if (cached) { free( cached ); cached = NULL; cached_size = 0; }
    if (!(source = load_tuxblox_icon( &width, &height ))) return NULL;
    if (!(scaled = malloc( (size_t)size * size * 4 )))
    {
        free( source );
        return NULL;
    }

    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            const int x0 = x * width / size, x1 = (x + 1) * width / size;
            const int y0 = y * height / size, y1 = (y + 1) * height / size;
            int sx, sy, channel, count = 0, total[4] = { 0, 0, 0, 0 };

            for (sy = y0; sy < (y1 > y0 ? y1 : y0 + 1); sy++)
            {
                for (sx = x0; sx < (x1 > x0 ? x1 : x0 + 1); sx++)
                {
                    const unsigned char *pixel = source + ((size_t)sy * width + sx) * 4;

                    /* Weighted by alpha, so the transparent edge does not drag
                     * whatever colour sits behind it into the visible part. */
                    for (channel = 0; channel < 3; channel++) total[channel] += pixel[channel] * pixel[3];
                    total[3] += pixel[3];
                    count++;
                }
            }
            for (channel = 0; channel < 3; channel++)
                scaled[((size_t)y * size + x) * 4 + channel] = total[3] ? total[channel] / total[3] : 0;
            scaled[((size_t)y * size + x) * 4 + 3] = count ? total[3] / count : 0;
        }
    }

    free( source );
    cached = scaled;
    cached_size = size;
    return cached;
}

/* TRUE once the logo is on the canvas, FALSE when there is none to draw and the
 * caller should fall back on the message icon Windows would have shown. */
static BOOL draw_logo( struct msgbox *box, int x, int y, int size )
{
    const unsigned char *pixels = logo_pixels( size );
    const unsigned int back = box->colors.back;
    XImage *image;
    char *data;
    int px, py;

    if (!pixels) return FALSE;
    if (!(data = malloc( (size_t)size * size * 4 ))) return FALSE;
    if (!(image = p_XCreateImage( box->display, box->visual, box->depth, ZPixmap, 0, data,
                                  size, size, 32, 0 )))
    {
        free( data );
        return FALSE;
    }

    for (py = 0; py < size; py++)
    {
        for (px = 0; px < size; px++)
        {
            const unsigned char *pixel = pixels + ((size_t)py * size + px) * 4;
            const int alpha = pixel[3];
            unsigned int color = 0;
            int channel;

            /* The source is blue, green, red, alpha; the canvas wants 0xRRGGBB. */
            for (channel = 0; channel < 3; channel++)
            {
                const int b = (back >> (channel * 8)) & 0xff;
                const int value = (pixel[channel] * alpha + b * (255 - alpha)) / 255;

                color |= (unsigned int)value << (channel * 8);
            }
            XPutPixel( image, px, py, x_pixel( box, color ) );
        }
    }

    p_XPutImage( box->display, box->canvas, box->gc, image, 0, 0, x, y, size, size );
    image->data = NULL;
    XDestroyImage( image );
    free( data );
    return TRUE;
}

static void draw_message_icon( struct msgbox *box, int x, int y );

/* The TuxBlox logo, or the mark Windows would have drawn when there is no logo
 * on this machine to load. */
static void draw_message_icon_at( struct msgbox *box, int x, int y )
{
    if (box->icon_size == MSGBOX_LOGO_SIZE && draw_logo( box, x, y, MSGBOX_LOGO_SIZE )) return;
    draw_message_icon( box, x, y );
}

static void draw_message_icon( struct msgbox *box, int x, int y )
{
    const int inset = MSGBOX_ICON_SIZE / 4;
    const int middle = x + MSGBOX_ICON_SIZE / 2;
    const unsigned int icon = box->flags & MSGBOX_ICON;
    unsigned int color = 0xe81123;
    const char *mark = NULL;

    switch (icon)
    {
    case MSGBOX_ICON_INFO:     color = 0x0078d4; mark = "i"; break;
    case MSGBOX_ICON_QUESTION: color = 0x0078d4; mark = "?"; break;
    case MSGBOX_ICON_WARNING:  color = 0xf7a800; mark = "!"; break;
    default: break;
    }

    p_XSetForeground( box->display, box->gc, x_pixel( box, color ) );
    p_XFillArc( box->display, box->canvas, box->gc, x, y, MSGBOX_ICON_SIZE, MSGBOX_ICON_SIZE, 0, 360 * 64 );

    if (!mark)
    {
        p_XSetForeground( box->display, box->gc, x_pixel( box, 0xffffff ) );
        p_XSetLineAttributes( box->display, box->gc, 3, LineSolid, CapRound, JoinRound );
        p_XDrawLine( box->display, box->canvas, box->gc, x + inset, y + inset,
                     x + MSGBOX_ICON_SIZE - inset, y + MSGBOX_ICON_SIZE - inset );
        p_XDrawLine( box->display, box->canvas, box->gc, x + MSGBOX_ICON_SIZE - inset, y + inset,
                     x + inset, y + MSGBOX_ICON_SIZE - inset );
        p_XSetLineAttributes( box->display, box->gc, 1, LineSolid, CapButt, JoinMiter );
        return;
    }

    draw_text( box, middle - text_width( box, mark, 1 ) / 2,
               y + (MSGBOX_ICON_SIZE - text_height( box )) / 2, mark, 1, 0xffffff );
}

/* Whether a point is inside a rounded rectangle, everything in subpixels. */
static BOOL in_round_rect( int x, int y, int width, int height, int radius )
{
    int cx, cy, dx, dy;

    if (x < 0 || y < 0 || x > width || y > height) return FALSE;
    cx = x < radius ? radius : (x > width - radius ? width - radius : x);
    cy = y < radius ? radius : (y > height - radius ? height - radius : y);
    dx = x - cx;
    dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

/* Put down in one piece as an image: X cannot fill a shape with antialiasing,
 * so each pixel is sampled sixteen times and the corners come out smooth
 * rather than stepped. */
static void draw_rounded_rect( struct msgbox *box, unsigned int fill, unsigned int edge,
                               int x, int y, int width, int height, int radius )
{
    const int step = MSGBOX_SUPERSAMPLE, samples = step * step;
    const unsigned int back = box->colors.back;
    XImage *image;
    char *data;
    int px, py, sx, sy;

    if (width <= 0 || height <= 0) return;
    if (!(data = malloc( (size_t)width * height * 4 ))) return;
    if (!(image = p_XCreateImage( box->display, box->visual, box->depth, ZPixmap, 0, data,
                                  width, height, 32, 0 )))
    {
        free( data );
        return;
    }

    for (py = 0; py < height; py++)
    {
        for (px = 0; px < width; px++)
        {
            int outer = 0, inner = 0, channel;
            unsigned int color = 0;

            for (sy = 0; sy < step; sy++)
            {
                for (sx = 0; sx < step; sx++)
                {
                    /* the middle of the subpixel, in units of half a subpixel */
                    const int fx = (px * step + sx) * 2 + 1;
                    const int fy = (py * step + sy) * 2 + 1;

                    if (in_round_rect( fx, fy, width * step * 2, height * step * 2, radius * step * 2 ))
                        outer++;
                    if (in_round_rect( fx - step * 2, fy - step * 2, (width - 2) * step * 2,
                                       (height - 2) * step * 2, (radius - 1) * step * 2 ))
                        inner++;
                }
            }

            for (channel = 16; channel >= 0; channel -= 8)
            {
                const int b = (back >> channel) & 0xff;
                const int e = (edge >> channel) & 0xff;
                const int f = (fill >> channel) & 0xff;
                const int value = (b * (samples - outer) + e * (outer - inner) + f * inner) / samples;

                color |= (unsigned int)value << channel;
            }
            XPutPixel( image, px, py, x_pixel( box, color ) );
        }
    }

    p_XPutImage( box->display, box->canvas, box->gc, image, 0, 0, x, y, width, height );
    image->data = NULL;
    XDestroyImage( image );
    free( data );
}

static void draw_button( struct msgbox *box, int index, int y )
{
    const struct msgbox_button *button = &box->buttons[index];
    const BOOL dark = is_dark_color( box->colors.back );
    const BOOL lit = index == box->hot;
    const unsigned int face = shade_color( box->colors.back, dark ? (lit ? 22 : 12) : (lit ? 34 : 22) );
    const unsigned int edge = index == box->focused ? box->colors.accent
                                                    : shade_color( box->colors.back, dark ? 34 : -22 );
    const int label_width = text_width( box, button->label, strlen(button->label) );

    draw_rounded_rect( box, face, edge, button->x, y, button->width, MSGBOX_BUTTON_HEIGHT,
                       MSGBOX_BUTTON_RADIUS );
    draw_text( box, button->x + (button->width - label_width) / 2,
               y + (MSGBOX_BUTTON_HEIGHT - text_height( box )) / 2,
               button->label, strlen(button->label), box->colors.button_text );
}

static void draw_box( struct msgbox *box )
{
    const int text_left = MSGBOX_MARGIN + box->icon_size + MSGBOX_ICON_GAP;
    const int buttons_top = box->height - MSGBOX_BUTTON_BOTTOM - MSGBOX_BUTTON_HEIGHT;
    int i, y, content_top, content_height = 0;

    fill_rect( box, box->colors.back, 0, 0, box->width, box->height );

    /* Heading and body together, centred against the logo beside them. */
    use_font( box, box->bold_font );
    if (box->heading) content_height += text_height( box ) + MSGBOX_HEADING_GAP;
    use_font( box, box->body_font );
    content_height += box->line_count * text_height( box );

    content_top = MSGBOX_MARGIN;
    if (content_height < box->icon_size) content_top += (box->icon_size - content_height) / 2;
    if (box->icon_size < content_height)
        draw_message_icon_at( box, MSGBOX_MARGIN, MSGBOX_MARGIN + (content_height - box->icon_size) / 2 );
    else
        draw_message_icon_at( box, MSGBOX_MARGIN, MSGBOX_MARGIN );

    y = content_top;
    if (box->heading)
    {
        use_font( box, box->bold_font );
        draw_text( box, text_left, y, box->heading, box->heading_len, box->colors.text );
        y += text_height( box ) + MSGBOX_HEADING_GAP;
        use_font( box, box->body_font );
    }

    for (i = 0; i < box->line_count; i++)
    {
        draw_text( box, text_left, y, box->lines[i].text, box->lines[i].len, box->colors.text );
        y += text_height( box );
    }

    for (i = 0; i < box->button_count; i++) draw_button( box, i, buttons_top );

    /* One copy of a finished picture, so moving the pointer over a button does
     * not show the box being drawn. */
    p_XCopyArea( box->display, box->canvas, box->window, box->gc, 0, 0, box->width, box->height, 0, 0 );
    p_XFlush( box->display );
}

static int button_at( struct msgbox *box, int x, int y )
{
    const int top = box->height - MSGBOX_BUTTON_BOTTOM - MSGBOX_BUTTON_HEIGHT;
    int i;

    if (y < top || y > top + MSGBOX_BUTTON_HEIGHT) return -1;
    for (i = 0; i < box->button_count; i++)
        if (x >= box->buttons[i].x && x <= box->buttons[i].x + box->buttons[i].width) return i;
    return -1;
}

static int measure_buttons( struct msgbox *box )
{
    int i, total = 0;

    for (i = 0; i < box->button_count; i++)
    {
        int width = text_width( box, box->buttons[i].label, strlen(box->buttons[i].label) ) +
                    MSGBOX_BUTTON_PADDING * 2;

        if (width < MSGBOX_BUTTON_MIN_WIDTH) width = MSGBOX_BUTTON_MIN_WIDTH;
        box->buttons[i].width = width;
        total += width + (i ? MSGBOX_BUTTON_GAP : 0);
    }
    return total;
}

static void place_buttons( struct msgbox *box )
{
    int i, x = box->width - MSGBOX_BUTTON_EDGE;

    for (i = box->button_count - 1; i >= 0; i--)
    {
        x -= box->buttons[i].width;
        box->buttons[i].x = x;
        x -= MSGBOX_BUTTON_GAP;
    }
}

/* The icon TuxBlox installs for itself, which the desktop already has a copy
 * of. Only reached when the program that raised the error has no icon. */
static unsigned char *load_tuxblox_icon( int *width, int *height )
{
    static const char *const sizes[] = { "48x48", "64x64", "32x32", "256x256" };
    const char *home = getenv( "HOME" );
    unsigned char *pixels = NULL;
    unsigned int i;

    if (!home) return NULL;

    for (i = 0; i < ARRAY_SIZE(sizes) && !pixels; i++)
    {
        char path[PATH_MAX];
        void *data;
        struct stat info;
        int fd;

        snprintf( path, sizeof(path), "%s/.local/share/icons/hicolor/%s/apps/tuxblox.png",
                  home, sizes[i] );
        if ((fd = open( path, O_RDONLY )) == -1) continue;
        if (!fstat( fd, &info ) && info.st_size > 0 && (data = malloc( info.st_size )))
        {
            if (read( fd, data, info.st_size ) == info.st_size)
                pixels = decode_png_icon( data, info.st_size, width, height );
            free( data );
        }
        close( fd );
    }
    return pixels;
}

static void set_window_icon( struct msgbox *box )
{
    const struct icon_group_entry *entries;
    BOOL bottom_up = FALSE;
    unsigned char *pixels = NULL;
    struct resources res;
    unsigned int tried = 0;
    const void *group;
    DWORD size = 0;
    long *property;
    int count, chosen, width = 0, height = 0, x, y;
    Atom net_wm_icon;

    if (find_resources( &res ) && (group = find_resource( &res, 14, -1, &size )) &&
        size >= 6 + sizeof(*entries))
    {
        count = ((const WORD *)group)[2];
        entries = (const struct icon_group_entry *)((const BYTE *)group + 6);
        if (size >= 6 + count * sizeof(*entries))
        {
            /* The best size first, then the next best if that one cannot be read. */
            while (!pixels && (chosen = pick_icon( entries, count, tried )) >= 0)
            {
                tried |= 1u << chosen;
                pixels = load_icon_pixels( &res, entries[chosen].id, &width, &height, &bottom_up );
            }
        }
    }

    if (!pixels)
    {
        bottom_up = FALSE;
        pixels = load_tuxblox_icon( &width, &height );
    }
    if (!pixels) return;
    pixels = scale_icon( pixels, &width, &height );

    if (!(property = malloc( (2 + (size_t)width * height) * sizeof(*property) )))
    {
        free( pixels );
        return;
    }

    property[0] = width;
    property[1] = height;
    for (y = 0; y < height; y++)
    {
        const unsigned char *row = pixels + (size_t)(bottom_up ? height - 1 - y : y) * width * 4;

        for (x = 0; x < width; x++)
            property[2 + y * width + x] = ((long)row[x * 4 + 3] << 24) | (row[x * 4 + 2] << 16) |
                                          (row[x * 4 + 1] << 8) | row[x * 4];
    }

    net_wm_icon = p_XInternAtom( box->display, "_NET_WM_ICON", False );
    p_XChangeProperty( box->display, box->window, net_wm_icon, XA_CARDINAL, 32, PropModeReplace,
                       (unsigned char *)property, 2 + width * height );
    free( property );
    free( pixels );
}


/* Asks the window manager for a dialog that cannot be resized and sits above
 * the program that raised it, which is what MB_TOPMOST gave. */
static void set_window_hints( struct msgbox *box, const char *title )
{
    Atom window_type, dialog, state, above;
    char program[128];
    XClassHint class_hint;
    XSizeHints hints;

    p_XStoreName( box->display, box->window, title );

    if (!tuxblox_program_name( program, sizeof(program) )) strcpy( program, "TuxBloxDialog" );
    class_hint.res_name = program;
    class_hint.res_class = program;
    p_XSetClassHint( box->display, box->window, &class_hint );

    memset( &hints, 0, sizeof(hints) );
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = box->width;
    hints.min_height = hints.max_height = box->height;
    p_XSetWMNormalHints( box->display, box->window, &hints );

    window_type = p_XInternAtom( box->display, "_NET_WM_WINDOW_TYPE", False );
    dialog = p_XInternAtom( box->display, "_NET_WM_WINDOW_TYPE_DIALOG", False );
    p_XChangeProperty( box->display, box->window, window_type, XA_ATOM, 32, PropModeReplace,
                       (unsigned char *)&dialog, 1 );

    state = p_XInternAtom( box->display, "_NET_WM_STATE", False );
    above = p_XInternAtom( box->display, "_NET_WM_STATE_ABOVE", False );
    p_XChangeProperty( box->display, box->window, state, XA_ATOM, 32, PropModeReplace,
                       (unsigned char *)&above, 1 );

    set_window_icon( box );

    box->delete_window = p_XInternAtom( box->display, "WM_DELETE_WINDOW", False );
    p_XSetWMProtocols( box->display, box->window, &box->delete_window, 1 );
}

static int run_dialog( const char *title, const char *body, unsigned int flags,
                       const struct msgbox_choices *set, int shown_fd, pid_t parent )
{
    struct msgbox box;
    char font_name[128];
    int text_width_used = 0, buttons_width, content_height, i, x, y;
    Window root;

    memset( &box, 0, sizeof(box) );
    box.pressed = MSGBOX_EXIT_DISMISSED;

    if (!(box.display = p_XOpenDisplay( NULL ))) return MSGBOX_EXIT_NOT_SHOWN;

    box.screen = p_XDefaultScreen( box.display );
    box.visual = p_XDefaultVisual( box.display, box.screen );
    box.colormap = p_XDefaultColormap( box.display, box.screen );
    box.depth = p_XDefaultDepth( box.display, box.screen );
    read_colors( &box.colors );

    read_font_name( font_name, sizeof(font_name) );
    if (p_XftFontOpenName)
    {
        char bold_name[sizeof(font_name) + 8];

        box.font = p_XftFontOpenName( box.display, box.screen, font_name );
        snprintf( bold_name, sizeof(bold_name), "%s:bold", font_name );
        box.bold_font = p_XftFontOpenName( box.display, box.screen, bold_name );
    }
    if (!box.font) box.core_font = p_XLoadQueryFont( box.display, "fixed" );
    box.body_font = box.font;
    if (!box.bold_font) box.bold_font = box.font;

    /* The caption leads the message, the way the launcher's own popups read it.
     * It is also the window title, which a box with no title bar never shows. */
    if (title && *title)
    {
        box.heading = title;
        box.heading_len = strlen( title );
    }

    /* Scaled here rather than at the first repaint, since the size it comes out
     * at decides how wide the box has to be. */
    box.icon_size = logo_pixels( MSGBOX_LOGO_SIZE ) ? MSGBOX_LOGO_SIZE : MSGBOX_ICON_SIZE;

    box.line_count = wrap_text( &box, body, MSGBOX_TEXT_MAX_WIDTH );
    for (i = 0; i < box.line_count; i++)
    {
        int width = text_width( &box, box.lines[i].text, box.lines[i].len );
        if (width > text_width_used) text_width_used = width;
    }
    if (box.heading)
    {
        int width;

        use_font( &box, box.bold_font );
        width = text_width( &box, box.heading, box.heading_len );
        use_font( &box, box.body_font );
        if (width > text_width_used) text_width_used = width;
    }

    box.flags = flags;
    for (i = 0; i < set->count; i++) box.buttons[box.button_count++].label = set->choice[i].label;
    box.focused = set->preferred;
    box.hot = -1;

    content_height = box.line_count * text_height( &box );
    if (box.heading)
    {
        use_font( &box, box.bold_font );
        content_height += text_height( &box ) + MSGBOX_HEADING_GAP;
        use_font( &box, box.body_font );
    }
    if (content_height < box.icon_size) content_height = box.icon_size;

    buttons_width = MSGBOX_BUTTON_EDGE * 2 + measure_buttons( &box );
    box.width = MSGBOX_MARGIN * 2 + box.icon_size + MSGBOX_ICON_GAP + text_width_used;
    if (box.width < buttons_width) box.width = buttons_width;
    box.height = MSGBOX_MARGIN + content_height + MSGBOX_CONTENT_GAP + MSGBOX_BUTTON_HEIGHT +
                 MSGBOX_BUTTON_BOTTOM;
    place_buttons( &box );

    x = (p_XDisplayWidth( box.display, box.screen ) - box.width) / 2;
    y = (p_XDisplayHeight( box.display, box.screen ) - box.height) / 2;
    root = p_XRootWindow( box.display, box.screen );
    box.window = p_XCreateSimpleWindow( box.display, root, x, y, box.width, box.height, 0, 0,
                                        x_pixel( &box, box.colors.back ) );
    box.canvas = p_XCreatePixmap( box.display, box.window, box.width, box.height, box.depth );
    box.gc = p_XCreateGC( box.display, box.window, 0, NULL );
    if (box.core_font) p_XSetFont( box.display, box.gc, box.core_font->fid );
    if (box.font) box.xft_draw = p_XftDrawCreate( box.display, box.canvas, box.visual, box.colormap );

    set_window_hints( &box, title );
    p_XSelectInput( box.display, box.window, ExposureMask | ButtonPressMask | ButtonReleaseMask |
                    KeyPressMask | PointerMotionMask | LeaveWindowMask );
    p_XMapRaised( box.display, box.window );

    /* The parent counts the message as delivered from here: whatever becomes of
     * this process now, the user has been shown the box and must not also get a
     * notification saying the same thing. */
    if (shown_fd != -1)
    {
        ssize_t written = write( shown_fd, "1", 1 );
        (void)written;
        close( shown_fd );
    }

    for (;;)
    {
        XEvent event;

        /* Waits for the next event, but not only for that: the box belongs to
         * the program that raised the error, and a program reporting a crash
         * often does not live to be answered. Nothing would close the box then,
         * so the parent is looked in on between events. */
        while (!p_XPending( box.display ))
        {
            struct pollfd pfd = { p_XConnectionNumber( box.display ), POLLIN, 0 };

            p_XFlush( box.display );
            if (parent && getppid() != parent) goto closed;
            poll( &pfd, 1, 500 );
        }

        p_XNextEvent( box.display, &event );
        if (event.type == Expose) draw_box( &box );
        /* Without this a keyboard layout that changed since the connection was
         * opened is read with the old one, and keys come out as other keys. */
        else if (event.type == MappingNotify) p_XRefreshKeyboardMapping( &event.xmapping );
        else if (event.type == ButtonRelease)
        {
            int index = button_at( &box, event.xbutton.x, event.xbutton.y );

            if (index >= 0)
            {
                box.pressed = index;
                break;
            }
        }
        else if (event.type == MotionNotify || event.type == LeaveNotify)
        {
            int index = event.type == LeaveNotify ? -1
                                                  : button_at( &box, event.xmotion.x, event.xmotion.y );

            if (index != box.hot)
            {
                box.hot = index;
                draw_box( &box );
            }
        }
        else if (event.type == KeyPress)
        {
            KeySym key = p_XLookupKeysym( &event.xkey, 0 );

            if (key == XK_Return || key == XK_KP_Enter || key == XK_space)
            {
                box.pressed = box.focused;
                break;
            }
            if (key == XK_Escape && set->can_dismiss) break;
            /* The arrows walk the buttons, as they do in a Windows dialog. */
            if (key == XK_Left || key == XK_Right || key == XK_Tab)
            {
                int step = key == XK_Left ? box.button_count - 1 : 1;

                box.focused = (box.focused + step) % box.button_count;
                draw_box( &box );
            }
        }
        else if (event.type == ClientMessage &&
                 (Atom)event.xclient.data.l[0] == box.delete_window) break;
    }

closed:
    if (box.xft_draw) p_XftDrawDestroy( box.xft_draw );
    p_XFreePixmap( box.display, box.canvas );
    p_XFreeGC( box.display, box.gc );
    p_XCloseDisplay( box.display );
    return box.pressed;
}

#endif /* SONAME_LIBX11 */


/* What became of the program drawing the box. A timeout is told apart from the
 * rest because by then it has been on screen for two minutes: the message was
 * delivered even though nobody answered it, so it must not be shown again. */
enum dialog_wait { DIALOG_ANSWERED, DIALOG_TIMED_OUT, DIALOG_LOST };

/* Waits for the box, but not for ever: this runs on the thread reporting the
 * error and there may be nobody in front of the machine to answer it. */
static enum dialog_wait wait_for_dialog( pid_t pid, int *status )
{
    ULONGLONG deadline = monotonic_counter() + 120000 * (ULONGLONG)10000;

    for (;;)
    {
        pid_t reaped = waitpid( pid, status, WNOHANG );

        if (reaped == pid) return DIALOG_ANSWERED;
        if (reaped == -1 && errno != EINTR) return DIALOG_LOST;
        if (monotonic_counter() >= deadline)
        {
            kill( pid, SIGTERM );
            waitpid( pid, status, 0 );
            return DIALOG_TIMED_OUT;
        }
        usleep( 50000 );
    }
}

/* The answer comes back down a pipe rather than as an exit code, because the
 * exit code can be lost -- a crashing program's own handler may reap the child
 * first -- and a lost answer would put the fallback box on screen underneath a
 * dialog that is already there. A byte can only come from a box that ran. */
static enum dialog_wait read_dialog_answer( int fd, pid_t pid, int *chosen )
{
    ULONGLONG deadline = monotonic_counter() + 120000 * (ULONGLONG)10000;

    for (;;)
    {
        struct pollfd waiting;
        int ready;

        waiting.fd = fd;
        waiting.events = POLLIN;
        waiting.revents = 0;
        ready = poll( &waiting, 1, 500 );
        if (ready == 1)
        {
            char answer;

            if (read( fd, &answer, 1 ) != 1) return DIALOG_LOST;
            *chosen = answer >= '0' && answer <= '9' ? answer - '0' : MSGBOX_DISMISSED;
            return DIALOG_ANSWERED;
        }
        if (ready == -1 && errno != EINTR) return DIALOG_LOST;
        if (monotonic_counter() >= deadline)
        {
            kill( pid, SIGTERM );
            return DIALOG_TIMED_OUT;
        }
    }
}

static BOOL launcher_at( char *path, size_t size, const char *dir )
{
    if (!dir || !*dir) return FALSE;
    if (snprintf( path, size, "%s/TuxBloxLauncher", dir ) >= (int)size) return FALSE;
    return access( path, X_OK ) == 0;
}

/* The install this library was loaded out of, which holds the launcher beside
 * the interface stack it draws with. Walked up to rather than counted back
 * from, so moving the libraries inside the install does not break it. */
static BOOL find_launcher( char *path, size_t size )
{
    const char *root = getenv( "TUXBLOX_ROOT" );
    const char *home = getenv( "HOME" );
    char dir[PATH_MAX];
    Dl_info info;
    int i;

    if (root && *root == '/' && launcher_at( path, size, root )) return TRUE;

    if (dladdr( tuxblox_show_message_box, &info ) && info.dli_fname &&
        realpath( info.dli_fname, dir ))
    {
        for (i = 0; i < 8; i++)
        {
            char *cut = strrchr( dir, '/' );

            if (!cut || cut == dir) break;
            *cut = 0;
            if (launcher_at( path, size, dir )) return TRUE;
        }
    }

    if (home && *home)
    {
        char fallback[PATH_MAX];

        if (snprintf( fallback, sizeof(fallback), "%s/.tuxblox", home ) < (int)sizeof(fallback) &&
            launcher_at( path, size, fallback )) return TRUE;
    }
    return FALSE;
}

/* The launcher draws every other window TuxBlox puts on screen, with the
 * interface libraries that ship beside it -- which this side cannot link
 * against -- so it is asked to draw this one too and to report the answer. */
static int run_launcher_dialog( const char *title, const char *body, const struct msgbox_choices *set )
{
    const char *args[16];
    char path[PATH_MAX], labels[128], preferred[8];
    int fds[2], icon_fd, status = 0, count = 0, chosen = MSGBOX_DISMISSED, answer, i, at = 0;
    enum dialog_wait waited;
    pid_t pid;

    if (!find_launcher( path, sizeof(path) )) return MSGBOX_NOT_SHOWN;
    if (pipe( fds ) == -1) return MSGBOX_NOT_SHOWN;

    /* The window carries the icon of the program that raised the error, the
     * way the box Windows draws does. */
    icon_fd = open_program_icon();

    for (i = 0; i < set->count && at < (int)sizeof(labels) - 1; i++)
        at += snprintf( labels + at, sizeof(labels) - at, "%s%s", i ? "," : "", set->choice[i].label );
    snprintf( preferred, sizeof(preferred), "%d", set->preferred );

    args[count++] = path;
    args[count++] = "--message-box";
    args[count++] = "--answer-fd";
    args[count++] = MSGBOX_ANSWER_FD_ARG;
    args[count++] = "--buttons";
    args[count++] = labels;
    args[count++] = "--default";
    args[count++] = preferred;
    /* Windows greys out the close button for a question with no way to say no, and this box is shut in the same cases. */
    if (!set->can_dismiss) args[count++] = "--no-dismiss";
    if (icon_fd != -1)
    {
        args[count++] = "--icon-fd";
        args[count++] = MSGBOX_ICON_FD_ARG;
    }
    args[count++] = title ? title : "";
    args[count++] = body ? body : "";
    args[count] = NULL;

    if ((pid = fork()) == -1)
    {
        close( fds[0] );
        close( fds[1] );
        if (icon_fd != -1) close( icon_fd );
        return MSGBOX_NOT_SHOWN;
    }
    if (!pid)
    {
        /* The launcher is a host program, and a loader path pointing at the
         * layer's own libraries would have it load those over the ones it was
         * built against. Wine puts the original back the same way for the
         * programs it starts on the desktop's behalf. */
        const char *original = getenv( "ORIG_LD_LIBRARY_PATH" );

        close( fds[0] );
        if (fds[1] != MSGBOX_ANSWER_FD)
        {
            if (dup2( fds[1], MSGBOX_ANSWER_FD ) == -1) _exit( MSGBOX_NOT_SHOWN );
            close( fds[1] );
        }
        if (icon_fd != -1 && icon_fd != MSGBOX_ICON_FD)
        {
            if (dup2( icon_fd, MSGBOX_ICON_FD ) == -1) _exit( MSGBOX_NOT_SHOWN );
            close( icon_fd );
        }
        if (original) setenv( "LD_LIBRARY_PATH", original, 1 );
        else unsetenv( "LD_LIBRARY_PATH" );
        unsetenv( "LD_PRELOAD" );
        execv( path, (char **)args );
        _exit( MSGBOX_NOT_SHOWN );
    }

    close( fds[1] );
    if (icon_fd != -1) close( icon_fd );
    waited = read_dialog_answer( fds[0], pid, &chosen );
    close( fds[0] );

    if (waited == DIALOG_ANSWERED) answer = chosen;
    else answer = waited == DIALOG_TIMED_OUT ? MSGBOX_DISMISSED : MSGBOX_NOT_SHOWN;

    /* Reaped however it went, so nothing is left behind. Its exit code says
     * only whether it had a screen to draw on, which is all there is left to
     * learn once no answer has come back. */
    if (wait_for_dialog( pid, &status ) == DIALOG_ANSWERED && waited == DIALOG_LOST &&
        WIFEXITED(status) && WEXITSTATUS(status) < 2)
        answer = MSGBOX_DISMISSED;

    return answer;
}

#ifdef SONAME_LIBX11

/* Drawn here instead, for an install the launcher is missing from. */
static int run_x11_dialog( const char *title, const char *body, unsigned int flags,
                           const struct msgbox_choices *set )
{
    int fds[2], status = 0;
    BOOL exited;
    char shown = 0;
    pid_t pid, parent;
    int code;

    if (!load_libraries()) return MSGBOX_NOT_SHOWN;
    if (pipe( fds ) == -1) fds[0] = fds[1] = -1;

    parent = getpid();
    if ((pid = fork()) == -1)
    {
        if (fds[0] != -1) { close( fds[0] ); close( fds[1] ); }
        return MSGBOX_NOT_SHOWN;
    }
    if (!pid)
    {
        if (fds[0] != -1) close( fds[0] );
        /* Forked rather than started, so without this it carries the name of
         * the program it came from and reads as a second copy of it. */
#if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
        prctl( PR_SET_NAME, "TuxBloxDialog" );
#endif

        _exit( run_dialog( title, body, flags, set, fds[1], parent ) );
    }
    if (fds[1] != -1) close( fds[1] );

    if (wait_for_dialog( pid, &status ) != DIALOG_ANSWERED) status = -1;
    if (fds[0] != -1)
    {
        ssize_t got = read( fds[0], &shown, 1 );

        if (got != 1) shown = 0;
        close( fds[0] );
    }

    /* The byte is written once the box is up, so a child that was killed
     * afterwards still counts as having asked the question. */
    exited = status != -1 && WIFEXITED(status);
    code = exited ? WEXITSTATUS(status) : MSGBOX_EXIT_NOT_SHOWN;
    if (!shown && code == MSGBOX_EXIT_NOT_SHOWN) return MSGBOX_NOT_SHOWN;
    return code < set->count ? code : MSGBOX_DISMISSED;
}

#endif /* SONAME_LIBX11 */


BOOL tuxblox_show_message_box( const char *title, const char *body, ULONG flags, ULONG *response )
{
    static int reentered;
    struct msgbox_choices set;
    int answer;

    /* A second message while one is on screen: the user is already being asked
     * about the first, and answers for both would pile up. */
    if (reentered) return FALSE;

    msgbox_set( flags, &set );
    reentered = 1;
    answer = run_launcher_dialog( title, body, &set );
#ifdef SONAME_LIBX11
    if (answer == MSGBOX_NOT_SHOWN) answer = run_x11_dialog( title, body, flags, &set );
#endif
    reentered = 0;

    if (answer == MSGBOX_NOT_SHOWN) return FALSE;
    /* Dismissed means the box was read and waved away, which is the same thing
     * the cancelling button says, and every set that can be dismissed has one. */
    if (response)
        *response = answer >= 0 && answer < set.count ? set.choice[answer].response
                                                      : set.choice[0].response;
    return TRUE;
}
