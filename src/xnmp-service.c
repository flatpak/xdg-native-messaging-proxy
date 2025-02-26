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

#include <libdex.h>

#include "xdg-native-messaging-proxy-dbus.h"
#include "xnmp-impl.h"

#include "xnmp-service.h"

struct _XnmpService
{
  XnmpDbusNativeMessagingProxySkeleton parent_instance;

  XnmpImpl *impl;
  GHashTable *cancellables; /* dbus unique name -> GCancellable */
};

#define XNMP_TYPE_SERVICE (xnmp_service_get_type ())
G_DECLARE_FINAL_TYPE (XnmpService,
                      xnmp_service,
                      XNMP, SERVICE,
                      XnmpDbusNativeMessagingProxySkeleton)

static void xnmp_native_messaging_proxy_iface_init (XnmpDbusNativeMessagingProxyIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (XnmpService,
                               xnmp_service,
                               XNMP_DBUS_TYPE_NATIVE_MESSAGING_PROXY_SKELETON,
                               G_IMPLEMENT_INTERFACE (XNMP_DBUS_TYPE_NATIVE_MESSAGING_PROXY,
                                                      xnmp_native_messaging_proxy_iface_init))

static GCancellable *
ensure_cancellable (XnmpService           *service,
                    GDBusMethodInvocation *invocation)
{
  const char *sender;
  g_autofree char *owned_sender = NULL;
  g_autoptr(GCancellable) cancellable = NULL;

  sender = g_dbus_method_invocation_get_sender (invocation);

  if (!g_hash_table_steal_extended (service->cancellables,
                                    sender,
                                    (gpointer *)&owned_sender,
                                    (gpointer *)&cancellable))
    {
      owned_sender = g_strdup (sender);
      cancellable = g_cancellable_new ();
    }

  g_hash_table_insert (service->cancellables,
                       g_steal_pointer (&owned_sender),
                       g_object_ref (cancellable));

  return cancellable;
}

static gboolean
handle_get_manifest (XnmpDbusNativeMessagingProxy *object,
                     GDBusMethodInvocation        *invocation,
                     const char                   *arg_messaging_host_name,
                     const char                   *arg_mode,
                     GVariant                     *arg_options)
{
  XnmpService *service = XNMP_SERVICE (object);
  DexFuture *fiber;
  DexFuture *cancellable;
  XnmpImplGetManifestData *data;

  data = xnmp_impl_get_manifest_data_new (service->impl,
                                          invocation,
                                          arg_messaging_host_name,
                                          arg_mode);

  fiber = dex_scheduler_spawn (NULL,
                               0,
                               (DexFiberFunc) xnmp_impl_handle_get_manifest,
                               data,
                               (GDestroyNotify) xnmp_impl_get_manifest_data_free);
  cancellable =
    dex_cancellable_new_from_cancellable (ensure_cancellable (service,
                                                              invocation));
  dex_future_disown (dex_future_all_race (fiber, cancellable, NULL));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_start (XnmpDbusNativeMessagingProxy *object,
              GDBusMethodInvocation        *invocation,
              GUnixFDList                  *fd_list,
              const char                   *arg_messaging_host_name,
              const char                   *arg_extension_or_origin,
              const char                   *arg_mode,
              GVariant                     *arg_options)
{
  XnmpService *service = XNMP_SERVICE (object);
  DexFuture *fiber;
  DexFuture *cancellable;
  XnmpImplStartData *data;

  data = xnmp_impl_start_data_new (service->impl,
                                   invocation,
                                   arg_messaging_host_name,
                                   arg_extension_or_origin,
                                   arg_mode);

  fiber = dex_scheduler_spawn (NULL,
                               0,
                               (DexFiberFunc) xnmp_impl_handle_start,
                               data,
                               (GDestroyNotify) xnmp_impl_start_data_free);
  cancellable =
    dex_cancellable_new_from_cancellable (ensure_cancellable (service,
                                                              invocation));
  dex_future_disown (dex_future_all_race (fiber, cancellable, NULL));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_close (XnmpDbusNativeMessagingProxy *object,
              GDBusMethodInvocation        *invocation,
              const char                   *arg_handle,
              GVariant                     *arg_options)
{
  XnmpService *service = XNMP_SERVICE (object);
  DexFuture *fiber;
  DexFuture *cancellable;
  XnmpImplCloseData *data;

  data = xnmp_impl_close_data_new (service->impl,
                                   invocation,
                                   arg_handle);

  fiber = dex_scheduler_spawn (NULL,
                               0,
                               (DexFiberFunc) xnmp_impl_handle_close,
                               data,
                               (GDestroyNotify) xnmp_impl_close_data_free);
  cancellable =
    dex_cancellable_new_from_cancellable (ensure_cancellable (service,
                                                              invocation));
  dex_future_disown (dex_future_all_race (fiber, cancellable, NULL));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}


static void
xnmp_native_messaging_proxy_iface_init (XnmpDbusNativeMessagingProxyIface *iface)
{
  iface->handle_get_manifest = handle_get_manifest;
  iface->handle_start = handle_start;
  iface->handle_close = handle_close;
}

static void
xnmp_service_dispose (GObject *object)
{
  XnmpService *service = XNMP_SERVICE (object);
  GHashTableIter iter;
  GCancellable *cancellable;

  g_hash_table_iter_init (&iter, service->cancellables);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &cancellable))
    g_cancellable_cancel (cancellable);

  g_clear_object (&service->impl);
  g_clear_pointer (&service->cancellables, g_hash_table_unref);

  G_OBJECT_CLASS (xnmp_service_parent_class)->dispose (object);
}

static void
xnmp_service_class_init (XnmpServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose  = xnmp_service_dispose;
}

static void
xnmp_service_init (XnmpService *service)
{
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  const char *name, *from, *to;
  XnmpService *service = XNMP_SERVICE (user_data);
  g_autofree char *owned_sender = NULL;
  g_autoptr(GCancellable) cancellable = NULL;

  g_variant_get (parameters, "(&s&s&s)", &name, &from, &to);

  if (name[0] != ':' ||
      strcmp (name, from) != 0 ||
      strcmp (to, "") != 0)
    return;

  if (g_hash_table_steal_extended (service->cancellables,
                                   name,
                                   (gpointer *)&owned_sender,
                                   (gpointer *)&cancellable))
    {
      g_info ("cancelling future for client %s", owned_sender);
      g_cancellable_cancel (cancellable);
    }
}

gboolean
init_xnmp_service (GDBusConnection  *connection,
                   GError          **error)
{
  g_autoptr(XnmpService) service = NULL;
  XnmpDbusNativeMessagingProxy *dbus_object;
  GDBusInterfaceSkeleton *skeleton;

  g_return_val_if_fail (g_object_get_data (G_OBJECT (connection),
                                           "-xnmp-service") == NULL,
                        FALSE);

  service = g_object_new (XNMP_TYPE_SERVICE, NULL);
  dbus_object = XNMP_DBUS_NATIVE_MESSAGING_PROXY (service);
  skeleton = G_DBUS_INTERFACE_SKELETON (service);
  service->impl = xnmp_impl_new (dbus_object);
  service->cancellables =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free, g_object_unref);

  g_dbus_connection_signal_subscribe (connection,
                                      DBUS_BUS_NAME,
                                      DBUS_IFACE,
                                      "NameOwnerChanged",
                                      DBUS_OBJECT_PATH,
                                      NULL,
                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_name_owner_changed,
                                      service, NULL);

  xnmp_dbus_native_messaging_proxy_set_version (dbus_object, 1);

  if (!g_dbus_interface_skeleton_export (skeleton,
                                         connection,
                                         XNMP_OBJECT_PATH,
                                         error))
    return FALSE;

  g_object_set_data_full (G_OBJECT (connection),
                          "-xnmp-service",
                          g_steal_pointer (&service),
                          (GDestroyNotify)g_object_unref);

  return TRUE;
}
