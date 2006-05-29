/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

 /*
 * File: cairofillbar.h
 * Copyright: newwen@gmail.com
 * Created on: Donnerstag, Mai 04 2006 00:07
 */

#ifndef CAIROFILLBAR_H_
#define CAIROFILLBAR_H_

#include <gtk/gtk.h>

/*the GObject boilerplate*/

G_BEGIN_DECLS

#define GB_TYPE_CAIRO_FILLBAR			(gb_cairo_fillbar_get_type ())

#define GB_CAIRO_FILLBAR(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST ((obj), \
		GB_TYPE_CAIRO_FILLBAR, GBCairoFillBar))

#define GB_CAIRO_FILLBAR_CLASS(obj) \
		(G_TYPE_CHECK_CLASS_CAST ((obj), \
		 GB_CAIRO_FILLBAR,  GBCairoFillBarClass))

#define GB_IS_CAIRO_FILLBAR(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
		 GB_TYPE_CAIRO_FILLBAR))

#define GB_IS_CAIRO_FILLBAR_CLASS(obj) \
 		(G_TYPE_CHECK_CLASS_TYPE ((obj), \
		GB_TYPE_CAIRO_FILLBAR))

#define GB_CAIRO_FILLBAR_GET_CLASS(obj) \
		(G_TYPE_INSTANCE_GET_CLASS ((obj), \
		 GB_TYPE_CAIRO_FILLBAR, GBCairoFillBarClass))

typedef struct _GBCairoFillBar GBCairoFillBar;
typedef struct _GBCairoFillBarClass GBCairoFillBarClass;
typedef struct _GBCairoFillBarPrivate GBCairoFillBarPrivate;


struct _GBCairoFillBar
{
	GtkDrawingArea parent;

	/* private */
	GBCairoFillBarPrivate *priv;
};


struct _GBCairoFillBarClass
{
	GtkDrawingAreaClass parent_class;
};


GtkWidget *gb_cairo_fillbar_new (void);
gchar *gb_cairo_fillbar_get_current_text (GBCairoFillBar *bar);
void gb_cairo_fillbar_set_project_total_size(GBCairoFillBar *bar, gdouble project_total_size);
void gb_cairo_fillbar_set_disk_size(GBCairoFillBar *bar, gdouble disk_size, gboolean is_time,
        gdouble tolerance_percent, gboolean allow_overburn);
gdouble gb_cairo_fillbar_get_disk_size(GBCairoFillBar *bar);
gboolean gb_cairo_fillbar_get_is_time(GBCairoFillBar *bar);
gdouble gb_cairo_fillbar_get_project_total_size(GBCairoFillBar *bar);


G_END_DECLS

#endif /*CAIROFILLBAR_H_*/
