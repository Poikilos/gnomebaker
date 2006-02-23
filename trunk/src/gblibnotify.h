/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 /*
 * File: gblibnotify.h
 * Copyright: Milen Dzhumerov <gamehack@1nsp1r3d.co.uk> Richard Hughes <richard@hughsie.com>
 */

#ifndef GB_LIBNOTITFY_H
#define GB_LIBNOTITFY_H

gboolean gblibnotify_init (const gchar *nice_name);
gboolean gblibnotify_clear (void);
gboolean gblibnotify_notification(const gchar *subject, const gchar *content);

#endif

