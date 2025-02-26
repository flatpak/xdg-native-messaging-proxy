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

#pragma once

#include <gio/gio.h>
#include <libdex.h>

#include "xdg-native-messaging-proxy-dbus.h"

#define DBUS_BUS_NAME "org.freedesktop.DBus"
#define DBUS_OBJECT_PATH "/org/freedesktop/DBus"
#define DBUS_IFACE DBUS_BUS_NAME

#define XNMP_BUS_NAME "org.freedesktop.NativeMessagingProxy"
#define XNMP_OBJECT_PATH "/org/freedesktop/nativemessagingproxy"
#define XNMP_IFACE XNMP_BUS_NAME

#define XNMP_TYPE_IMPL (xnmp_impl_get_type ())
G_DECLARE_FINAL_TYPE (XnmpImpl,
                      xnmp_impl,
                      XNMP, IMPL,
                      GObject)

XnmpImpl * xnmp_impl_new (XnmpDbusNativeMessagingProxy *dbus_object);

typedef struct _XnmpImplGetManifestData XnmpImplGetManifestData;
typedef struct _XnmpImplStartData XnmpImplStartData;
typedef struct _XnmpImplCloseData XnmpImplCloseData;

XnmpImplGetManifestData * xnmp_impl_get_manifest_data_new (XnmpImpl              *impl,
                                                           GDBusMethodInvocation *invocation,
                                                           const char            *messaging_host_name,
                                                           const char            *mode);

void xnmp_impl_get_manifest_data_free (XnmpImplGetManifestData *d);

DexFuture * xnmp_impl_handle_get_manifest (XnmpImplGetManifestData *data);

XnmpImplStartData * xnmp_impl_start_data_new (XnmpImpl              *impl,
                                              GDBusMethodInvocation *invocation,
                                              const char            *messaging_host_name,
                                              const char            *extension_or_origin,
                                              const char            *mode);

void xnmp_impl_start_data_free (XnmpImplStartData *d);

DexFuture * xnmp_impl_handle_start (XnmpImplStartData *data);

XnmpImplCloseData * xnmp_impl_close_data_new (XnmpImpl              *impl,
                                              GDBusMethodInvocation *invocation,
                                              const char            *handle);

void xnmp_impl_close_data_free (XnmpImplCloseData *d);

DexFuture * xnmp_impl_handle_close (XnmpImplCloseData *data);
