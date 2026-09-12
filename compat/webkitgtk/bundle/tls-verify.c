// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

/* webkitgtk-bundle/tls-verify.c
 *
 * Real end-to-end TLS verification for the glib-networking/GnuTLS backend
 * built by build-in-container.sh -- not just "the .so files exist", but
 * genuine evidence a TLS handshake and certificate validation happened:
 *
 *   1. Confirms GIO's default TLS backend is a real, non-dummy implementation
 *      (GTlsBackendGnutls), via g_tls_backend_get_default().
 *   2. Makes an actual HTTPS GET request via libsoup3 against a real URL.
 *   3. Inspects the negotiated GTlsCertificate and its validation-error
 *      bitmask to confirm the certificate was actually checked against the
 *      configured trust store, not merely that the request didn't crash.
 *
 * libsoup has no TLS implementation of its own in any configuration -- it
 * always delegates to GIO's pluggable TLS backend at runtime, which
 * glib-networking provides as a dynamically loaded GIO module
 * ($PREFIX/lib/x86_64-linux-gnu/gio/modules/libgiognutls.so). Without that
 * module actually being found and loaded, GIO silently falls back to a dummy
 * backend that fails every TLS operation -- this test exists specifically to
 * catch that failure mode, which a plain "does libsoup link" or
 * "does pkg-config report a version" check cannot.
 *
 * Re-run this against the webkitgtk-prefix volume with:
 *
 *   webkitgtk-bundle/verify-tls.sh
 *
 * (compiles this file against the volume's own libsoup-3.0/gio-2.0 inside
 * the tuxblox-webkitgtk-builder container and runs it -- see that script for
 * the exact podman invocation.)
 */
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>

int
main (int argc, char **argv)
{
  const char *url = argc > 1 ? argv[1] : "https://example.com";

  /* --- Step 1: GIO TLS backend identity --- */
  GTlsBackend *backend = g_tls_backend_get_default ();
  const char *backend_type = G_OBJECT_TYPE_NAME (backend);
  gboolean backend_supports_tls = g_tls_backend_supports_tls (backend);
  printf ("GTlsBackend implementation: %s\n", backend_type);
  printf ("g_tls_backend_supports_tls(): %s\n", backend_supports_tls ? "TRUE" : "FALSE");

  if (g_str_has_prefix (backend_type, "GDummyTls") || !backend_supports_tls)
    {
      fprintf (stderr, "FAIL: dummy/no-op TLS backend -- glib-networking's real "
                        "backend was not found/registered (check GIO_MODULE_DIR / "
                        "GIO_EXTRA_MODULES and that libgiognutls.so is present).\n");
      return 1;
    }

  /* --- Step 2: real HTTPS request through libsoup, using that backend --- */
  SoupSession *session = soup_session_new ();
  SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, url);
  if (!msg)
    {
      fprintf (stderr, "FAIL: could not construct SoupMessage for %s\n", url);
      return 1;
    }

  GError *error = NULL;
  GBytes *body = soup_session_send_and_read (session, msg, NULL, &error);

  if (error)
    {
      fprintf (stderr, "FAIL: HTTPS request to %s errored: %s\n", url, error->message);
      return 1;
    }

  GTlsCertificate *cert = soup_message_get_tls_peer_certificate (msg);
  guint status = soup_message_get_status (msg);

  printf ("HTTP status: %u %s\n", status, soup_message_get_reason_phrase (msg));
  printf ("Response body size: %zu bytes\n", g_bytes_get_size (body));

  if (!cert)
    {
      fprintf (stderr, "FAIL: soup_message_get_tls_peer_certificate() returned NULL "
                        "-- no TLS session was actually negotiated.\n");
      return 1;
    }

  GTlsCertificateFlags cert_errors = soup_message_get_tls_peer_certificate_errors (msg);
  printf ("TLS peer certificate present: YES\n");
  printf ("TLS certificate validation errors bitmask: %u (0 == fully trusted)\n", cert_errors);

  gchar *cert_pem = NULL;
  g_object_get (cert, "certificate-pem", &cert_pem, NULL);
  if (cert_pem)
    {
      printf ("Peer certificate PEM length: %zu bytes (real cert data received)\n",
              strlen (cert_pem));
      g_free (cert_pem);
    }

  g_bytes_unref (body);
  g_object_unref (msg);
  g_object_unref (session);

  if (cert_errors != 0)
    {
      fprintf (stderr, "FAIL: certificate validation errors present (0x%x) -- CA "
                        "trust store isn't resolving correctly.\n", cert_errors);
      return 1;
    }

  printf ("PASS: real HTTPS request completed with a fully-trusted TLS certificate.\n");
  return 0;
}
