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

#define DEMO_WIN32_WINDOW_CLASS_NAME L"demo_wc"

struct demo_window_win32
{
    struct demo_window w;

    HINSTANCE instance;
    HWND window;
};

#ifndef VKD3D_CROSSTEST
static VkSurfaceKHR demo_window_win32_create_vk_surface(struct demo_window *window, VkInstance vk_instance)
{
    struct demo_window_win32 *window_win32 = CONTAINING_RECORD(window, struct demo_window_win32, w);
    struct VkWin32SurfaceCreateInfoKHR surface_desc;
    VkSurfaceKHR vk_surface;

    surface_desc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_desc.pNext = NULL;
    surface_desc.flags = 0;
    surface_desc.hinstance = window_win32->instance;
    surface_desc.hwnd = window_win32->window;
    if (vkCreateWin32SurfaceKHR(vk_instance, &surface_desc, NULL, &vk_surface) < 0)
        return VK_NULL_HANDLE;

    return vk_surface;
}
#endif

static void demo_window_win32_destroy(struct demo_window *window)
{
    struct demo_window_win32 *window_win32 = CONTAINING_RECORD(window, struct demo_window_win32, w);

    DestroyWindow(window_win32->window);
}

static void demo_window_win32_destroyed(struct demo_window *window)
{
    struct demo_window_win32 *window_win32 = CONTAINING_RECORD(window, struct demo_window_win32, w);

    demo_window_cleanup(&window_win32->w);
    free(window_win32);
}

static struct demo_window *demo_window_win32_create(struct demo *demo, const char *title,
        unsigned int width, unsigned int height, void *user_data)
{
    struct demo_window_win32 *window_win32;
    RECT rect = {0, 0, width, height};
    int title_size;
    WCHAR *title_w;
    DWORD style;

    if (!(window_win32 = malloc(sizeof(*window_win32))))
        return NULL;

#ifdef VKD3D_CROSSTEST
    if (!demo_window_init(&window_win32->w, demo, user_data))
#else
    if (!demo_window_init(&window_win32->w, demo, user_data,
            demo_window_win32_create_vk_surface, demo_window_win32_destroy))
#endif
    {
        free(window_win32);
        return NULL;
    }

    title_size = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    if (!(title_w = calloc(title_size, sizeof(*title_w))))
    {
        demo_window_cleanup(&window_win32->w);
        free(window_win32);
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w, title_size);

    window_win32->instance = GetModuleHandle(NULL);

    style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    AdjustWindowRect(&rect, style, FALSE);
    window_win32->window = CreateWindowExW(0, DEMO_WIN32_WINDOW_CLASS_NAME, title_w, style, CW_USEDEFAULT,
            CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, window_win32->instance, NULL);
    free(title_w);
    if (!window_win32->window)
    {
        demo_window_cleanup(&window_win32->w);
        free(window_win32);
        return NULL;
    }
    SetWindowLongPtrW(window_win32->window, GWLP_USERDATA, (LONG_PTR)window_win32);

    return &window_win32->w;
}

static void demo_win32_get_dpi(struct demo *demo, double *dpi_x, double *dpi_y)
{
    struct demo_win32 *win32 = &demo->u.win32;

    *dpi_x = *dpi_y = win32->GetDpiForSystem();
}

static demo_key demo_key_from_win32_vkey(DWORD vkey)
{
    static const struct
    {
        DWORD vkey;
        demo_key demo_key;
    }
    lookup[] =
    {
        {VK_OEM_MINUS,  '-'},
        {VK_OEM_PLUS,   '='},
        {VK_ESCAPE,     DEMO_KEY_ESCAPE},
        {VK_LEFT,       DEMO_KEY_LEFT},
        {VK_UP,         DEMO_KEY_UP},
        {VK_RIGHT,      DEMO_KEY_RIGHT},
        {VK_DOWN,       DEMO_KEY_DOWN},
        {VK_ADD,        DEMO_KEY_KP_ADD},
        {VK_SUBTRACT,   DEMO_KEY_KP_SUBTRACT},
        {VK_F1,         DEMO_KEY_F1},
    };
    unsigned int i;

    if (vkey >= '0' && vkey <= '9')
        return vkey;
    if (vkey >= 'A' && vkey <= 'Z')
        return vkey + 0x20;

    for (i = 0; i < ARRAY_SIZE(lookup); ++i)
    {
        if (lookup[i].vkey == vkey)
            return lookup[i].demo_key;
    }

    return DEMO_KEY_UNKNOWN;
}

static LRESULT CALLBACK demo_win32_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct demo_window_win32 *window_win32 = (void *)GetWindowLongPtrW(window, GWLP_USERDATA);

    switch (message)
    {
        case WM_PAINT:
            if (window_win32 && window_win32->w.expose_func)
                window_win32->w.expose_func(&window_win32->w, window_win32->w.user_data);
            return 0;

        case WM_KEYDOWN:
            if (!window_win32->w.key_press_func)
                break;
            window_win32->w.key_press_func(&window_win32->w,
                    demo_key_from_win32_vkey(wparam), window_win32->w.user_data);
            return 0;

        case WM_DESTROY:
            demo_window_win32_destroyed(&window_win32->w);
            return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

static void demo_win32_process_events(struct demo *demo)
{
    MSG msg = {0};

    for (;;)
    {
        if (!demo->idle_func)
        {
            if (GetMessageW(&msg, NULL, 0, 0) == -1)
                break;
        }
        else if (!PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            demo->idle_func(demo, demo->user_data);
            continue;
        }

        if (msg.message == WM_QUIT)
            break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!demo->window_count)
            PostQuitMessage(0);
    }
}

static void demo_win32_cleanup(struct demo *demo)
{
    UnregisterClassW(DEMO_WIN32_WINDOW_CLASS_NAME, GetModuleHandle(NULL));
}

static inline UINT demo_win32_GetDpiForSystem(void)
{
    return 96;
}

static bool demo_win32_init(struct demo_win32 *win32)
{
    WNDCLASSEXW wc;

    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = demo_win32_window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = DEMO_WIN32_WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIconW(NULL, IDI_WINLOGO);
    if (!RegisterClassExW(&wc))
        return false;

    if ((win32->GetDpiForSystem = (void *)GetProcAddress(GetModuleHandleA("user32"), "GetDpiForSystem")))
        SetProcessDPIAware();
    else
        win32->GetDpiForSystem = demo_win32_GetDpiForSystem;

    return true;
}
