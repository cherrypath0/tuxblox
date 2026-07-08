/*
 * Copyright 2025 Henri Verbeet
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

typedef long NSInteger;
typedef unsigned long NSUInteger;

typedef struct NSPoint
{
    double x, y;
} NSPoint;

typedef struct NSRect
{
    double x, y;
    double w, h;
} NSRect;

#define BOOL OBJC_BOOL
#include "private/appkit.h"
#include "private/foundation.h"
#include "private/quartzcore.h"
#undef BOOL

extern const id NSDefaultRunLoopMode;

enum NSBackingStoreType
{
    NSBackingStoreBuffered = 2,
};

enum NSEventType
{
    NSEventTypeKeyDown = 0xa,
    NSEventTypeApplicationDefined = 0xf,
};

enum NSWindowStyleMask
{
    NSWindowStyleMaskBorderless             = 0x0000,
    NSWindowStyleMaskTitled                 = 0x0001,
    NSWindowStyleMaskClosable               = 0x0002,
    NSWindowStyleMaskMiniaturizable         = 0x0004,
    NSWindowStyleMaskResizable              = 0x0008,
    NSWindowStyleMaskUtilityWindow          = 0x0010,
    NSWindowStyleMaskDocModalWindow         = 0x0040,
    NSWindowStyleMaskNonactivatingPanel     = 0x0080,
    NSWindowStyleMaskUnifiedTitleAndToolbar = 0x1000,
    NSWindowStyleMaskHUDWindow              = 0x2000,
    NSWindowStyleMaskFullScreen             = 0x4000,
    NSWindowStyleMaskFullSizeContentView    = 0x8000,
};

enum
{
    DemoWindowDestroyed,
};

struct demo_window_macos
{
    struct demo_window w;
    id window;
    id layer;
};

static struct demo_window_macos *demo_macos_find_macos_window(struct demo *demo, id window)
{
    size_t i;

    for (i = 0; i < demo->window_count; ++i)
    {
        struct demo_window_macos *window_macos = CONTAINING_RECORD(demo->windows[i], struct demo_window_macos, w);

        if (window_macos->window == window)
            return window_macos;
    }

    return NULL;
}

static VkSurfaceKHR demo_window_macos_create_vk_surface(struct demo_window *window, VkInstance vk_instance)
{
    struct demo_window_macos *window_macos = CONTAINING_RECORD(window, struct demo_window_macos, w);
    struct VkMetalSurfaceCreateInfoEXT surface_desc;
    VkSurfaceKHR vk_surface;
    id l, v;

    l = window_macos->layer = CAMetalLayer_layer();
    CAMetalLayer_setContentsScale(l, NSScreen_backingScaleFactor(NSScreen_mainScreen()));
    v = NSWindow_contentView(window_macos->window);
    NSView_setLayer(v, l);
    NSView_setWantsLayer(v, true);

    surface_desc.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    surface_desc.pNext = NULL;
    surface_desc.flags = 0;
    surface_desc.pLayer = l;
    if (vkCreateMetalSurfaceEXT(vk_instance, &surface_desc, NULL, &vk_surface) < 0)
        return VK_NULL_HANDLE;

    return vk_surface;
}

static void demo_window_macos_destroy(struct demo_window *window)
{
    struct demo_window_macos *window_macos = CONTAINING_RECORD(window, struct demo_window_macos, w);

    NSWindow_close(window_macos->window);
}

static void demo_window_macos_destroyed(struct demo_window_macos *window_macos)
{
    CAMetalLayer_release(window_macos->layer);
    NSWindow_release(window_macos->window);
    demo_window_cleanup(&window_macos->w);
    free(window_macos);
}

static struct demo_window *demo_window_macos_create(struct demo *demo,
        const char *title, unsigned int width, unsigned int height, void *user_data)
{
    unsigned long style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
    struct demo_window_macos *window_macos;
    NSRect r = {0, 0, width, height};
    double scale;
    id w, s;

    if (!(window_macos = malloc(sizeof(*window_macos))))
        return NULL;

    if (!demo_window_init(&window_macos->w, demo, user_data,
            demo_window_macos_create_vk_surface, demo_window_macos_destroy))
    {
        free(window_macos);
        return NULL;
    }

    s = NSScreen_mainScreen();
    scale = NSScreen_backingScaleFactor(s);
    r.w /= scale;
    r.h /= scale;

    w = window_macos->window = class_createInstance(objc_getClass("DemoWindow"), 0);
    NSWindow_initWithContentRect(w, r, style, NSBackingStoreBuffered, true, s);
    NSWindow_setReleasedWhenClosed(w, false);
    NSWindow_setDelegate(w, w);
    NSWindow_center(w);
    NSWindow_setTitle(w, NSString_stringWithUTF8String(title));

    NSWindow_makeKeyAndOrderFront(w, nil);
    window_macos->layer = nil;

    return &window_macos->w;
}

static void demo_macos_get_dpi(struct demo *demo, double *dpi_x, double *dpi_y)
{
    *dpi_x = *dpi_y = 96.0 * NSScreen_backingScaleFactor(NSScreen_mainScreen());
}

static demo_key demo_key_from_nsevent(id event)
{
    enum vkey
    {
        kVK_ANSI_A              = 0x00,
        kVK_ANSI_F              = 0x03,
        kVK_ANSI_W              = 0x0d,
        kVK_ANSI_Equal          = 0x18,
        kVK_ANSI_Minus          = 0x1b,
        kVK_Escape              = 0x35,
        kVK_ANSI_KeypadPlus     = 0x45,
        kVK_ANSI_KeypadMinus    = 0x4e,
        kVK_F1                  = 0x7a,
        kVK_LeftArrow           = 0x7b,
        kVK_RightArrow          = 0x7c,
        kVK_DownArrow           = 0x7d,
        kVK_UpArrow             = 0x7e,
    } vkey;
    size_t i;

    static const struct
    {
        enum vkey vkey;
        demo_key demo_key;
    }
    lookup[] =
    {
        {kVK_ANSI_A,            'a'},
        {kVK_ANSI_F,            'f'},
        {kVK_ANSI_W,            'w'},
        {kVK_ANSI_Equal,        '='},
        {kVK_ANSI_Minus,        '-'},
        {kVK_Escape,            DEMO_KEY_ESCAPE},
        {kVK_ANSI_KeypadPlus,   DEMO_KEY_KP_ADD},
        {kVK_ANSI_KeypadMinus,  DEMO_KEY_KP_SUBTRACT},
        {kVK_F1,                DEMO_KEY_F1},
        {kVK_LeftArrow,         DEMO_KEY_LEFT},
        {kVK_RightArrow,        DEMO_KEY_RIGHT},
        {kVK_DownArrow,         DEMO_KEY_DOWN},
        {kVK_UpArrow,           DEMO_KEY_UP},
    };

    vkey = NSEvent_keyCode(event);
    for (i = 0; i < ARRAY_SIZE(lookup); ++i)
    {
        if (lookup[i].vkey == vkey)
            return lookup[i].demo_key;
    }

    return DEMO_KEY_UNKNOWN;
}

static void demo_macos_process_events(struct demo *demo)
{
    struct demo_window_macos *window_macos;
    struct demo_window *window;
    id a, event;
    size_t i;

    for (i = 0; i < demo->window_count; ++i)
    {
        if ((window = demo->windows[i])->expose_func)
            window->expose_func(window, window->user_data);
    }

    a = NSApplication_sharedApplication();
    while (demo->window_count)
    {
        if (!demo->idle_func)
        {
            if (!(event = NSApplication_nextEventMatchingMask(a, ~(uint64_t)0,
                    NSDate_distantFuture(), NSDefaultRunLoopMode, true)))
                break;
        }
        else if (!(event = NSApplication_nextEventMatchingMask(a, ~(uint64_t)0, nil, NSDefaultRunLoopMode, true)))
        {
            demo->idle_func(demo, demo->user_data);
            continue;
        }

        switch (NSEvent_type(event))
        {
            case NSEventTypeKeyDown:
                if (NSMenu_performKeyEquivalent(NSApplication_mainMenu(a), event))
                    continue;
                if (!(window_macos = demo_macos_find_macos_window(demo, NSEvent_window(event)))
                        || !window_macos->w.key_press_func)
                    break;
                window_macos->w.key_press_func(&window_macos->w,
                        demo_key_from_nsevent(event), window_macos->w.user_data);
                continue;

            case NSEventTypeApplicationDefined:
                if (NSEvent_subtype(event) != DemoWindowDestroyed
                        || !(window_macos = demo_macos_find_macos_window(demo, NSEvent_window(event))))
                    break;
                demo_window_macos_destroyed(window_macos);
                continue;
        }

        NSApplication_sendEvent(a, event);
    }
}

static void DemoWindow_windowWillClose(id window, SEL sel, id notification)
{
    id event;

    event = NSEvent_otherEventWithType(NSEventTypeApplicationDefined, (NSPoint){0.0, 0.0},
            0, 0.0, NSWindow_windowNumber(window), nil, DemoWindowDestroyed, 0, 0);
    NSApplication_postEvent(NSApplication_sharedApplication(), event, true);
}

static void demo_macos_cleanup(struct demo *demo)
{
}

static bool demo_macos_init(struct demo_macos *macos)
{
    id application, item, menu, submenu;
    Class c;

    if ((c = objc_allocateClassPair(objc_getClass("NSWindow"), "DemoWindow", 0)))
    {
        class_addMethod(c, sel_registerName("windowWillClose:"), (IMP)DemoWindow_windowWillClose, "v@:@");
        objc_registerClassPair(c);
    }

    application = NSApplication_sharedApplication();
    NSApplication_setActivationPolicy(application, 0);

    menu = NSMenu_new();

    submenu = NSMenu_new();
    NSMenu_addItemWithTitle(submenu, NSString_stringWithUTF8String("Quit"),
            sel_registerName("terminate:"), NSString_stringWithUTF8String("q"));

    item = NSMenuItem_new();
    NSMenuItem_setSubmenu(item, submenu);
    NSMenu_release(submenu);
    NSMenu_addItem(menu, item);
    NSMenuItem_release(item);

    NSApplication_setMainMenu(application, menu);
    NSMenu_release(menu);

    NSApplication_finishLaunching(application);

    return true;
}
