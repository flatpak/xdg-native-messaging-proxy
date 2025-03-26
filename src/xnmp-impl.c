/*
 * Copyright © 2022-2025 Canonical Ltd
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdint.h>
#include <libdex.h>
#include <json-glib/json-glib.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>

#include "xnmp-impl.h"

typedef enum _XnmpImplMode
{
  XNMP_IMPL_MODE_CHROMIUM,
  XNMP_IMPL_MODE_MOZILLA,
} XnmpImplMode;

struct _XnmpImpl {
  GObject parent_instance;

  XnmpDbusNativeMessagingProxy *dbus_object;
  GHashTable *running; /* handle -> GCancellable */
  GStrv chromium_search_paths;
  GStrv mozilla_search_paths;
};

G_DEFINE_FINAL_TYPE (XnmpImpl, xnmp_impl, G_TYPE_OBJECT);

typedef struct _XnmpImplGetManifestData
{
  XnmpImpl *impl;
  GDBusMethodInvocation *invocation;
  char *messaging_host_name;
  char *mode;
} XnmpImplGetManifestData;

XnmpImplGetManifestData *
xnmp_impl_get_manifest_data_new (XnmpImpl              *impl,
                                 GDBusMethodInvocation *invocation,
                                 const char            *messaging_host_name,
                                 const char            *mode)
{
  XnmpImplGetManifestData *d = g_new0 (XnmpImplGetManifestData, 1);
  d->impl = g_object_ref (impl);
  d->invocation = g_object_ref (invocation);
  d->messaging_host_name = g_strdup (messaging_host_name);
  d->mode = g_strdup (mode);

  return d;
}

void
xnmp_impl_get_manifest_data_free (XnmpImplGetManifestData *d)
{
  g_clear_object (&d->impl);
  g_clear_object (&d->invocation);
  g_clear_pointer (&d->messaging_host_name, g_free);
  g_clear_pointer (&d->mode, g_free);
  g_free (d);
}

typedef struct _XnmpImplStartData
{
  XnmpImpl *impl;
  GDBusMethodInvocation *invocation;
  char *messaging_host_name;
  char *extension_or_origin;
  char *mode;
} XnmpImplStartData;

XnmpImplStartData *
xnmp_impl_start_data_new (XnmpImpl              *impl,
                          GDBusMethodInvocation *invocation,
                          const char            *messaging_host_name,
                          const char            *extension_or_origin,
                          const char            *mode)
{
  XnmpImplStartData *d = g_new0 (XnmpImplStartData, 1);
  d->impl = g_object_ref (impl);
  d->invocation = g_object_ref (invocation);
  d->messaging_host_name = g_strdup (messaging_host_name);
  d->extension_or_origin = g_strdup (extension_or_origin);
  d->mode = g_strdup (mode);

  return d;
}

void
xnmp_impl_start_data_free (XnmpImplStartData *d)
{
  g_clear_object (&d->impl);
  g_clear_object (&d->invocation);
  g_clear_pointer (&d->messaging_host_name, g_free);
  g_clear_pointer (&d->extension_or_origin, g_free);
  g_clear_pointer (&d->mode, g_free);
  g_free (d);
}

typedef struct _XnmpImplCloseData
{
  XnmpImpl *impl;
  GDBusMethodInvocation *invocation;
  char *handle;
} XnmpImplCloseData;

XnmpImplCloseData *
xnmp_impl_close_data_new (XnmpImpl              *impl,
                          GDBusMethodInvocation *invocation,
                          const char            *handle)
{
  XnmpImplCloseData *d = g_new0 (XnmpImplCloseData, 1);
  d->impl = g_object_ref (impl);
  d->invocation = g_object_ref (invocation);
  d->handle = g_strdup (handle);

  return d;
}

void
xnmp_impl_close_data_free (XnmpImplCloseData *d)
{
  g_clear_object (&d->impl);
  g_clear_object (&d->invocation);
  g_clear_pointer (&d->handle, g_free);
  g_free (d);
}

static GStrv
xnmp_impl_get_search_paths (XnmpImpl     *impl,
                            XnmpImplMode  mode)
{
  switch (mode)
    {
    case XNMP_IMPL_MODE_CHROMIUM:
      return impl->chromium_search_paths;
    case XNMP_IMPL_MODE_MOZILLA:
      return impl->mozilla_search_paths;
      break;
    }
  g_assert_not_reached ();
}

XnmpImplMode
get_mode_from_str (const char *mode)
{
  if (g_strcmp0 (mode, "mozilla") == 0)
    return XNMP_IMPL_MODE_MOZILLA;

  if (g_strcmp0 (mode, "chromium") == 0)
    return XNMP_IMPL_MODE_CHROMIUM;

  return XNMP_IMPL_MODE_MOZILLA;
}

static gboolean
is_valid_name (const char *name)
{
  /* This regexp comes from the Mozilla documentation on valid native
     messaging host names:

     https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests#native_messaging_manifests

     That is, one or more dot-separated groups composed of
     alphanumeric characters and underscores.
  */
  return g_regex_match_simple ("^\\w+(\\.\\w+)*$", name, 0, 0);
}

static gboolean
is_valid_manifest (JsonParser  *parser,
                   const char  *messaging_host_name,
                   GError     **error)
{
  JsonObject *metadata_root;
  const char *value;

  metadata_root = json_node_get_object (json_parser_get_root (parser));

  value = json_object_get_string_member (metadata_root, "name");
  if (g_strcmp0 (value, messaging_host_name) != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   "Metadata contains an unexpected name");
      return FALSE;
    }

  value = json_object_get_string_member (metadata_root, "type");
  if (g_strcmp0 (value, "stdio") != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   "Not a \"stdio\" type native messaging host");
      return FALSE;
    }

  value = json_object_get_string_member (metadata_root, "path");
  if (!g_path_is_absolute (value))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   "Native messaging host path is not absolute");
      return FALSE;
    }

  return TRUE;
}

static GBytes *
find_manifest (XnmpImpl      *impl,
               const char    *messaging_host_name,
               XnmpImplMode   mode,
               char         **manifest_filename_out,
               JsonParser   **json_parser_out,
               GError       **error)
{
  GStrv search_paths = NULL;
  g_autofree char *metadata_basename = NULL;

  /* Check that the we have a valid native messaging host name */
  if (!is_valid_name (messaging_host_name))
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "Invalid native messaging host name");
      return NULL;
    }

  search_paths = xnmp_impl_get_search_paths (impl, mode);
  metadata_basename = g_strconcat (messaging_host_name, ".json", NULL);

  for (size_t i = 0; search_paths[i] != NULL; i++)
    {
      g_autoptr (GFile) metadata_file =
        g_file_new_build_filename (search_paths[i],
                                   metadata_basename,
                                   NULL);
      g_autoptr (GBytes) contents = NULL;
      g_autoptr(JsonParser) parser = NULL;
      gconstpointer data;
      gsize size;
      g_autoptr (GError) local_error = NULL;

      contents = dex_await_boxed (dex_file_load_contents_bytes (metadata_file),
                                  &local_error);
      if (!contents)
        {
          if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            {
              g_warning ("Loading file %s failed: %s",
                         g_file_peek_path (metadata_file),
                         local_error->message);
            }

            g_debug ("Skipping file %s", g_file_peek_path (metadata_file));
            continue;
        }

      parser = json_parser_new ();
      data = g_bytes_get_data (contents, &size);

      if (!json_parser_load_from_data (parser, data, size, &local_error))
        {
          g_warning ("Manifest %s is not a valid JSON file: %s",
                     g_file_peek_path (metadata_file),
                     local_error->message);

            g_debug ("Skipping file %s", g_file_peek_path (metadata_file));
          continue;
        }

      if (!is_valid_manifest (parser, messaging_host_name, &local_error))
        {
          g_warning ("Manifest %s is invalid: %s",
                     g_file_peek_path (metadata_file),
                     local_error->message);

            g_debug ("Skipping file %s", g_file_peek_path (metadata_file));
          continue;
        }

      g_debug ("Found manifest %s", g_file_peek_path (metadata_file));

      if (manifest_filename_out)
        *manifest_filename_out = g_file_get_path (metadata_file);

      if (json_parser_out)
        *json_parser_out = g_steal_pointer (&parser);

      return g_steal_pointer (&contents);
    }

  g_debug ("Requested manifest not found");
  g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_NOT_FOUND,
               "Could not find native messaging host");

  return NULL;
}

DexFuture *
xnmp_impl_handle_get_manifest (XnmpImplGetManifestData *data)
{
  XnmpImpl *impl = data->impl;
  GDBusMethodInvocation *invocation = data->invocation;
  const char *messaging_host_name = data->messaging_host_name;
  XnmpImplMode mode = get_mode_from_str (data->mode);
  g_autoptr (GBytes) manifest = NULL;
  g_autoptr (GError) error = NULL;

  g_print ("Handling GetManifest %s (%s)\n", messaging_host_name, data->mode);

  manifest = find_manifest (impl, messaging_host_name, mode, NULL, NULL, &error);
  if (!manifest)
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return NULL;
    }

  xnmp_dbus_native_messaging_proxy_complete_get_manifest (impl->dbus_object,
                                                          invocation,
                                                          g_bytes_get_data (manifest, NULL));

  return NULL;
}

static GSubprocess *
subprocess_new_with_pipes (const char * const  *argv,
                           int                 *stdin_fd_out,
                           int                 *stdout_fd_out,
                           int                 *stderr_fd_out,
                           GError             **error)
{
  g_autoptr (GSubprocess) subp = NULL;
  GInputStream *stream_stdout, *stream_stderr;
  GOutputStream *stream_stdin;

  subp = g_subprocess_newv ((const char * const *)argv,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE |
                            G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                            G_SUBPROCESS_FLAGS_STDERR_PIPE,
                            error);
  if (!subp)
    return NULL;

  stream_stdin = g_subprocess_get_stdin_pipe (subp);
  stream_stdout = g_subprocess_get_stdout_pipe (subp);
  stream_stderr = g_subprocess_get_stderr_pipe (subp);

  *stdin_fd_out =
    g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (stream_stdin));
  *stdout_fd_out =
    g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (stream_stdout));
  *stderr_fd_out =
    g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (stream_stderr));

  return g_steal_pointer (&subp);
}

static void
register_running (XnmpImpl      *impl,
                  const char   **object_path_out,
                  GCancellable **cancellable_out)
{
  g_autofree char *object_path = NULL;
  g_autoptr (GCancellable) cancellable = NULL;

  do {
    uint64_t key;

    g_clear_pointer (&object_path, g_free);

    key = g_random_int ();
    key = (key << 32) | g_random_int ();
    object_path = g_strdup_printf (XNMP_OBJECT_PATH "/%" G_GUINT64_FORMAT, key);
  }
  while (g_hash_table_contains (impl->running, object_path));

  g_debug ("registering running messaging host handle: %s", object_path);

  cancellable = g_cancellable_new ();

  *cancellable_out = cancellable;
  *object_path_out = object_path;

  g_hash_table_insert (impl->running,
                       g_steal_pointer (&object_path),
                       g_steal_pointer (&cancellable));
}

static void
unregister_running (XnmpImpl   *impl,
                    const char *object_path)
{
  g_debug ("unregistering running messaging host handle: %s", object_path);

  g_hash_table_remove (impl->running, object_path);
}

static void
cancel_running (XnmpImpl   *impl,
                const char *object_path)
{
  GCancellable *cancellable;

  cancellable = g_hash_table_lookup (impl->running, object_path);
  if (cancellable)
    {
      g_debug ("canceling %s\n", object_path);
      g_cancellable_cancel (cancellable);
    }
}

DexFuture *
xnmp_impl_handle_start (XnmpImplStartData *data)
{
  XnmpImpl *impl = data->impl;
  GDBusMethodInvocation *invocation = data->invocation;
  const char *messaging_host_name = data->messaging_host_name;
  const char *extension_or_origin = data->extension_or_origin;
  XnmpImplMode mode = get_mode_from_str (data->mode);
  g_autoptr (GBytes) manifest = NULL;
  g_autoptr (JsonParser) manifest_json = NULL;
  g_autofree char *manifest_filename = NULL;
  JsonObject *metadata_root;
  const char *argv[4];
  size_t i = 0;
  gboolean success;
  GSubprocess *subp;
  int subp_pipes[3];
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_auto(GVariantBuilder) closed_options_builder =
    G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
  const char *handle;
  GCancellable *cancellable;
  g_autoptr (GError) error = NULL;

  g_print ("Handling Start %s (%s)\n", messaging_host_name, data->mode);

  manifest = find_manifest (impl,
                            messaging_host_name,
                            mode,
                            &manifest_filename,
                            &manifest_json,
                            &error);
  if (!manifest)
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return NULL;
    }

  /* Chromium:
   * https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging
   *
   * Mozilla:
   * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging
   * https://searchfox.org/mozilla-central/rev/9fcc11127fbfbdc88cbf37489dac90542e141c77/toolkit/components/extensions/NativeMessaging.sys.mjs#104-110
   */

  metadata_root = json_node_get_object (json_parser_get_root (manifest_json));
  argv[i++] = json_object_get_string_member (metadata_root, "path");

  if (mode == XNMP_IMPL_MODE_MOZILLA)
    argv[i++] = manifest_filename;

  argv[i++] = extension_or_origin;

  argv[i++] = NULL;

  g_assert (i <= G_N_ELEMENTS (argv));

  g_debug ("Spawning native messaging host %s\n", argv[0]);

  subp = subprocess_new_with_pipes ((const char * const *)argv,
                                    &subp_pipes[0],
                                    &subp_pipes[1],
                                    &subp_pipes[2],
                                    &error);
  if (!subp)
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return NULL;
    }

  register_running (impl, &handle, &cancellable);

  fd_list = g_unix_fd_list_new_from_array ((const int *)&subp_pipes,
                                           G_N_ELEMENTS (subp_pipes));
  xnmp_dbus_native_messaging_proxy_complete_start (impl->dbus_object,
                                                   invocation,
                                                   fd_list,
                                                   g_variant_new_handle (0),
                                                   g_variant_new_handle (1),
                                                   g_variant_new_handle (2),
                                                   handle);

  success = dex_await (dex_future_all_race (dex_subprocess_wait_check (subp),
                                            dex_cancellable_new_from_cancellable (cancellable),
                                            NULL),
                       &error);
  if (!success)
    g_debug ("native messaging host failed: %s\n", error->message);
  g_clear_error (&error);

  g_subprocess_force_exit (subp);

  g_debug ("Emitting Closed signal on %s\n", g_dbus_method_invocation_get_sender (invocation));
  if (!g_dbus_connection_emit_signal (g_dbus_method_invocation_get_connection (invocation),
                                      g_dbus_method_invocation_get_sender (invocation),
                                      XNMP_OBJECT_PATH,
                                      XNMP_IFACE,
                                      "Closed",
                                      g_variant_new ("(oa{sv})",
                                                     handle,
                                                     &closed_options_builder),
                                      &error))
    g_warning ("Failed emitting Closed signal: %s", error->message);
  g_clear_error (&error);

  unregister_running (impl, handle);

  return NULL;
}

DexFuture *
xnmp_impl_handle_close (XnmpImplCloseData *data)
{
  XnmpImpl *impl = data->impl;
  GDBusMethodInvocation *invocation = data->invocation;
  const char *handle = data->handle;

  g_print ("Handling Close %s\n", handle);

  cancel_running (impl, handle);

  xnmp_dbus_native_messaging_proxy_complete_close (impl->dbus_object,
                                                   invocation);

  return NULL;
}

static void
ensure_manifest_search_paths (XnmpImpl *impl)
{
  const char *hosts_path_str;
  g_autoptr(GPtrArray) search_paths = NULL;

  hosts_path_str = g_getenv ("XNMP_HOST_LOCATIONS");
  if (hosts_path_str != NULL)
    {
      impl->chromium_search_paths = g_strsplit (hosts_path_str, ":", -1);
      impl->mozilla_search_paths = g_strsplit (hosts_path_str, ":", -1);
      return;
    }

  /* Chrome and Chromium search paths documented here:
   * https://developer.chrome.com/docs/extensions/nativeMessaging/#native-messaging-host-location
   */
  search_paths = g_ptr_array_new_with_free_func (g_free);
  /* Add per-user directories */
  g_ptr_array_add (search_paths, g_build_filename (g_get_user_config_dir (), "google-chrome", "NativeMessagingHosts", NULL));
  g_ptr_array_add (search_paths, g_build_filename (g_get_user_config_dir (), "chromium", "NativeMessagingHosts", NULL));
  /* Add system wide directories */
  g_ptr_array_add (search_paths, g_strdup ("/etc/opt/chrome/native-messaging-hosts"));
  g_ptr_array_add (search_paths, g_strdup ("/etc/chromium/native-messaging-hosts"));
  /* And the same for the configured prefix */
  g_ptr_array_add (search_paths, g_strdup (SYSCONFDIR "/opt/chrome/native-messaging-hosts"));
  g_ptr_array_add (search_paths, g_strdup (SYSCONFDIR "/chromium/native-messaging-hosts"));
  /* NULL terminated */
  g_ptr_array_add (search_paths, NULL);
  impl->chromium_search_paths =
    (GStrv) g_ptr_array_free (g_steal_pointer (&search_paths), FALSE);

  /* Firefox search paths documented here:
   * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests#manifest_location
   */
  search_paths = g_ptr_array_new_with_free_func (g_free);
  /* Add per-user directories */
  g_ptr_array_add (search_paths, g_build_filename (g_get_home_dir (), ".mozilla", "native-messaging-hosts", NULL));
  g_ptr_array_add (search_paths, g_build_filename (g_get_user_config_dir (), "mozilla", "native-messaging-hosts", NULL));
  /* Add system wide directories */
  g_ptr_array_add (search_paths, g_strdup ("/usr/lib/mozilla/native-messaging-hosts"));
  g_ptr_array_add (search_paths, g_strdup ("/usr/lib64/mozilla/native-messaging-hosts"));
  /* And the same for the configured prefix.
     This is helpful on Debian-based systems where LIBDIR is
     suffixed with 'dpkg-architecture -qDEB_HOST_MULTIARCH',
     e.g. '/usr/lib/x86_64-linux-gnu'.
     https://salsa.debian.org/debian/debhelper/-/blob/5b96b19b456fe5e094f2870327a753b4b3ece0dc/lib/Debian/Debhelper/Buildsystem/meson.pm#L78
   */
  g_ptr_array_add (search_paths, g_strdup (LIBDIR "/mozilla/native-messaging-hosts"));
  /* NULL terminated */
  g_ptr_array_add (search_paths, NULL);
  impl->mozilla_search_paths =
    (GStrv) g_ptr_array_free (g_steal_pointer (&search_paths), FALSE);
}

static void
xnmp_impl_dispose (GObject *object)
{
  XnmpImpl *impl = XNMP_IMPL (object);

  g_clear_object (&impl->dbus_object);
  g_clear_pointer (&impl->chromium_search_paths, g_strfreev);
  g_clear_pointer (&impl->mozilla_search_paths, g_strfreev);
  g_clear_pointer (&impl->running, g_hash_table_unref);

  G_OBJECT_CLASS (xnmp_impl_parent_class)->dispose (object);
}

static void
xnmp_impl_init (XnmpImpl *impl)
{
}

static void
xnmp_impl_class_init (XnmpImplClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose  = xnmp_impl_dispose;
}

XnmpImpl *
xnmp_impl_new (XnmpDbusNativeMessagingProxy *dbus_object)
{
  XnmpImpl *impl = g_object_new (XNMP_TYPE_IMPL, NULL);

  impl->dbus_object = g_object_ref (dbus_object);
  impl->running = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);

  ensure_manifest_search_paths (impl);

  return impl;
}

