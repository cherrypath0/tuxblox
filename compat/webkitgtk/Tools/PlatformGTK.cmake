if (DEVELOPER_MODE)
    if (EXISTS "${CMAKE_SOURCE_DIR}/flatpak")
        add_subdirectory(flatpak)
    endif ()
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI/glib)
endif ()

if (ENABLE_MINIBROWSER)
  add_subdirectory(MiniBrowser/gtk)
endif ()
