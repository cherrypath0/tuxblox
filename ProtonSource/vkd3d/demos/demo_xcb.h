/*
 * Copyright 2016 Józef Kucia for CodeWeavers
 * Copyright 2016 Henri Verbeet for CodeWeavers
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

struct demo_window_xcb
{
    struct demo_window w;

    xcb_window_t window;
};

static xcb_screen_t *demo_xcb_get_screen(xcb_connection_t *c, int idx)
{
    xcb_screen_iterator_t iter;

    iter = xcb_setup_roots_iterator(xcb_get_setup(c));
    for (; iter.rem; xcb_screen_next(&iter), --idx)
    {
        if (!idx)
            return iter.data;
    }

    return NULL;
}

static xcb_atom_t demo_xcb_get_atom(xcb_connection_t *c, const char *name)
{
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = XCB_NONE;

    cookie = xcb_intern_atom(c, 0, strlen(name), name);
    if ((reply = xcb_intern_atom_reply(c, cookie, NULL)))
    {
        atom = reply->atom;
        free(reply);
    }

    return atom;
}

static struct demo_window_xcb *demo_xcb_find_xcb_window(struct demo *demo, xcb_window_t window)
{
    size_t i;

    for (i = 0; i < demo->window_count; ++i)
    {
        struct demo_window_xcb *window_xcb = CONTAINING_RECORD(demo->windows[i], struct demo_window_xcb, w);

        if (window_xcb->window == window)
            return window_xcb;
    }

    return NULL;
}

static VkSurfaceKHR demo_window_xcb_create_vk_surface(struct demo_window *window, VkInstance vk_instance)
{
    struct demo_window_xcb *window_xcb = CONTAINING_RECORD(window, struct demo_window_xcb, w);
    struct VkXcbSurfaceCreateInfoKHR surface_desc;
    VkSurfaceKHR vk_surface;

    surface_desc.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surface_desc.pNext = NULL;
    surface_desc.flags = 0;
    surface_desc.connection = window->demo->u.xcb.connection;
    surface_desc.window = window_xcb->window;
    if (vkCreateXcbSurfaceKHR(vk_instance, &surface_desc, NULL, &vk_surface) < 0)
        return VK_NULL_HANDLE;

    return vk_surface;
}

static void demo_window_xcb_destroy(struct demo_window *window)
{
    struct demo_window_xcb *window_xcb = CONTAINING_RECORD(window, struct demo_window_xcb, w);
    struct demo_xcb *xcb = &window->demo->u.xcb;

    xcb_destroy_window(xcb->connection, window_xcb->window);
    xcb_flush(xcb->connection);
    demo_window_cleanup(&window_xcb->w);
    free(window_xcb);
}

static struct demo_window *demo_window_xcb_create(struct demo *demo, const char *title,
        unsigned int width, unsigned int height, void *user_data)
{
    struct demo_xcb *xcb = &demo->u.xcb;
    struct demo_window_xcb *window_xcb;
    xcb_size_hints_t hints;
    xcb_screen_t *screen;

    static const uint32_t window_events = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;

    if (!(screen = demo_xcb_get_screen(xcb->connection, xcb->screen)))
        return NULL;

    if (!(window_xcb = malloc(sizeof(*window_xcb))))
        return NULL;

    if (!demo_window_init(&window_xcb->w, demo, user_data,
            demo_window_xcb_create_vk_surface, demo_window_xcb_destroy))
    {
        free(window_xcb);
        return NULL;
    }

    window_xcb->window = xcb_generate_id(xcb->connection);
    xcb_create_window(xcb->connection, XCB_COPY_FROM_PARENT, window_xcb->window, screen->root, 0, 0,
            width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
            XCB_CW_EVENT_MASK, &window_events);
    xcb_change_property(xcb->connection, XCB_PROP_MODE_REPLACE, window_xcb->window, XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING, 8, strlen(title), title);
    xcb_change_property(xcb->connection, XCB_PROP_MODE_REPLACE, window_xcb->window, xcb->wm_protocols_atom,
            XCB_ATOM_ATOM, 32, 1, &xcb->wm_delete_window_atom);
    hints.flags = XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
    hints.min_width = width;
    hints.min_height = height;
    hints.max_width = width;
    hints.max_height = height;
    xcb_change_property(xcb->connection, XCB_PROP_MODE_REPLACE, window_xcb->window, XCB_ATOM_WM_NORMAL_HINTS,
            XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(hints) >> 2, &hints);

    xcb_map_window(xcb->connection, window_xcb->window);

    xcb_flush(xcb->connection);

    return &window_xcb->w;
}

static void demo_xcb_get_dpi(struct demo *demo, double *dpi_x, double *dpi_y)
{
    struct demo_xcb *xcb = &demo->u.xcb;
    xcb_screen_t *screen;

    if (!(screen = demo_xcb_get_screen(xcb->connection, xcb->screen)))
    {
        *dpi_x = *dpi_y = 96.0;
        return;
    }

    *dpi_x = screen->width_in_pixels * 25.4 / screen->width_in_millimeters;
    *dpi_y = screen->height_in_pixels * 25.4 / screen->height_in_millimeters;
}

static void demo_xcb_process_events(struct demo *demo)
{
    const struct xcb_client_message_event_t *client_message;
    struct xcb_key_press_event_t *key_press;
    struct demo_xcb *xcb = &demo->u.xcb;
    struct demo_window_xcb *window_xcb;
    xcb_generic_event_t *event;
    xcb_keysym_t sym;

    xcb_flush(xcb->connection);

    while (demo->window_count)
    {
        if (!demo->idle_func)
        {
            if (!(event = xcb_wait_for_event(xcb->connection)))
                break;
        }
        else if (!(event = xcb_poll_for_event(xcb->connection)))
        {
            demo->idle_func(demo, demo->user_data);
            continue;
        }

        switch (XCB_EVENT_RESPONSE_TYPE(event))
        {
            case XCB_EXPOSE:
                if ((window_xcb = demo_xcb_find_xcb_window(demo, ((struct xcb_expose_event_t *)event)->window))
                        && window_xcb->w.expose_func)
                    window_xcb->w.expose_func(&window_xcb->w, window_xcb->w.user_data);
                break;

            case XCB_KEY_PRESS:
                key_press = (struct xcb_key_press_event_t *)event;
                if (!(window_xcb = demo_xcb_find_xcb_window(demo, key_press->event)) || !window_xcb->w.key_press_func)
                    break;
                sym = xcb_key_press_lookup_keysym(xcb->xcb_keysyms, key_press, 0);
                window_xcb->w.key_press_func(&window_xcb->w, sym, window_xcb->w.user_data);
                break;

            case XCB_CLIENT_MESSAGE:
                client_message = (xcb_client_message_event_t *)event;
                if (client_message->type == xcb->wm_protocols_atom
                        && client_message->data.data32[0] == xcb->wm_delete_window_atom
                        && (window_xcb = demo_xcb_find_xcb_window(demo, client_message->window)))
                    demo_window_xcb_destroy(&window_xcb->w);
                break;
        }

        free(event);
    }
}

static void demo_xcb_cleanup(struct demo *demo)
{
    struct demo_xcb *xcb = &demo->u.xcb;

    xcb_key_symbols_free(xcb->xcb_keysyms);
    xcb_disconnect(xcb->connection);
}

static bool demo_xcb_init(struct demo_xcb *xcb)
{
    if (!(xcb->connection = xcb_connect(NULL, &xcb->screen)))
        return false;
    if (xcb_connection_has_error(xcb->connection) > 0)
        goto fail;
    if ((xcb->wm_delete_window_atom = demo_xcb_get_atom(xcb->connection, "WM_DELETE_WINDOW")) == XCB_NONE)
        goto fail;
    if ((xcb->wm_protocols_atom = demo_xcb_get_atom(xcb->connection, "WM_PROTOCOLS")) == XCB_NONE)
        goto fail;
    if (!(xcb->xcb_keysyms = xcb_key_symbols_alloc(xcb->connection)))
        goto fail;

    return true;

fail:
    xcb_disconnect(xcb->connection);
    return false;
}
