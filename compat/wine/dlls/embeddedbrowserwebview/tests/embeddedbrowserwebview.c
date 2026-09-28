/*
 * Unit tests for embeddedbrowserwebview
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

#include <windows.h>
#include "wine/test.h"

HRESULT WINAPI CreateWebViewEnvironmentWithOptionsInternal(BOOL,BOOL,PCWSTR,void*,void*);
HRESULT WINAPI CreateSharedWebViewEnvironmentInternal(void**);
HRESULT WINAPI DllCanUnloadNow(void);
void * WINAPI GetHandleVerifier(void);

/* A loader that hands us no completion handler has nowhere to receive the
 * environment, so this must come back as an error rather than a crash. */
static void test_null_handler(void)
{
    HRESULT hr = CreateWebViewEnvironmentWithOptionsInternal( TRUE, FALSE, NULL, NULL, NULL );
    ok( FAILED(hr), "expected failure, got %#lx\n", hr );
}

static void test_shared_environment_is_not_implemented(void)
{
    void *env = (void *)0xdeadbeef;
    HRESULT hr = CreateSharedWebViewEnvironmentInternal( &env );
    ok( hr == E_NOTIMPL, "got %#lx\n", hr );
}

/* S_FALSE keeps a loader from unloading us while an environment is alive. */
static void test_can_unload_now(void)
{
    ok( DllCanUnloadNow() == S_FALSE, "expected S_FALSE\n" );
}

static void test_handle_verifier(void)
{
    ok( GetHandleVerifier() == NULL, "expected NULL\n" );
}

START_TEST(embeddedbrowserwebview)
{
    test_null_handler();
    test_shared_environment_is_not_implemented();
    test_can_unload_now();
    test_handle_verifier();
}
