/*
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

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libdex.h>

#include "xnmp-impl.h"
#include "xnmp-service.h"

static int global_exit_status = 0;
static GMainLoop *loop = NULL;

static gboolean opt_replace;
static gboolean opt_show_version;

static GOptionEntry entries[] = {
  { "replace", 'r', 0, G_OPTION_ARG_NONE, &opt_replace, "Replace a running instance", NULL },
  { "version", 0, 0, G_OPTION_ARG_NONE, &opt_show_version, "Show program version.", NULL},
  { NULL }
};

static void
exit_with_status (int status)
{
  g_debug ("Exiting with status %d", status);

  global_exit_status = status;
  g_main_loop_quit (loop);
}

static gboolean
on_sighub_signal (gpointer user_data)
{
  g_debug ("Received SIGHUB");

  exit_with_status (0);
  return G_SOURCE_REMOVE;
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  g_debug ("Bus name %s acquired", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  g_debug ("Bus name %s lost", name);

  exit_with_status (0);
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  g_autoptr(GError) error = NULL;

  g_debug ("Bus %s acquired", name);

  if (!init_xnmp_service (connection, &error))
    {
      g_critical ("No document portal: %s", error->message);
      return exit_with_status (1);
    }
}

int
main (int argc, char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GSource) signal_handler_source = NULL;
  g_autoptr(GDBusConnection) session_bus = NULL;
  GBusNameOwnerFlags flags;
  guint owner_id;
  g_autoptr(GError) error = NULL;

  if (g_getenv ("XNMP_WAIT_FOR_DEBUGGER") != NULL)
    {
      g_printerr ("\nnative messaging proxy (PID %d) is waiting for a debugger. "
                  "Use `gdb -p %d` to connect. \n",
                  getpid (), getpid ());

      if (raise (SIGSTOP) == -1)
        {
          g_printerr ("Failed waiting for debugger\n");
          exit (1);
        }

      raise (SIGCONT);
    }

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new ("- native messaging proxy");
  g_option_context_set_summary (context, "A proxy for native messaging IPC");
  g_option_context_set_description (context,
    "native messaging proxy allows sandboxed applications to retrieve "
    "manifests and start those native messaging hosts. This proxy is not secure "
    "Any native messaging host might provide functionality to escape the "
    "sandbox.");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s", g_get_application_name (), error->message);
      g_printerr ("\n");
      g_printerr ("Try \"%s --help\" for more information.", g_get_prgname ());
      g_printerr ("\n");
      return 1;
    }

  if (opt_show_version)
    {
      g_print (PACKAGE_STRING "\n");
      return 0;
    }

  g_set_prgname (argv[0]);

  dex_init ();

  loop = g_main_loop_new (NULL, FALSE);

  signal_handler_source = g_unix_signal_source_new (SIGHUP);
  g_source_set_callback (signal_handler_source,
                         G_SOURCE_FUNC (on_sighub_signal),
                         NULL, NULL);
  g_source_attach (signal_handler_source, g_main_loop_get_context (loop));

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (session_bus == NULL)
    {
      g_printerr ("No session bus: %s", error->message);
      return 2;
    }

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
          (opt_replace ? G_BUS_NAME_OWNER_FLAGS_REPLACE : 0);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             XNMP_BUS_NAME,
                             flags,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);
  g_main_loop_unref (loop);

  return global_exit_status;
}
