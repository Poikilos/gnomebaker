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
 * File: cairofillbar.c
 * Copyright: newwen@gmail.com
 * Created on: Donnerstag, Mai 04 2006 00:02
 */
 
#include <math.h>
#include <gtk/gtk.h> 
#include "cairofillbar.h"
#include "gbcommon.h"


G_DEFINE_TYPE (GBCairoFillBar, gb_cairo_fillbar, GTK_TYPE_DRAWING_AREA);

#define GB_CAIRO_FILLBAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GB_TYPE_CAIRO_FILLBAR, GBCairoFillBarPrivate))


struct _GBCairoFillBarPrivate
{
	gint64 disk_size;
	gboolean is_time; 			/*whether the size is time or bytes*/
	gint64 project_total_size;
	gboolean allow_overbun; 	/*for the text-display logic*/
	gdouble tolerance_percent;
};


typedef enum
{
	 TOP_LEFT_CORNER = 1 << 0,
	 TOP_RIGHT_CORNER = 1 << 1, 
	 BOTTOM_RIGHT_CORNER = 1 << 2,
	 BOTTOM_LEFT_CORNER = 1 << 3,
	 ALL_CORNERS = TOP_LEFT_CORNER | TOP_RIGHT_CORNER | BOTTOM_RIGHT_CORNER | BOTTOM_LEFT_CORNER
} RectangleCorners;


typedef struct
{
	double r;
	double g;
	double b;
	double a;
} CairoColor;


/*it´s usefull to define some colors*/
static const GdkColor RED =    { 0, 0xffff, 0     , 0      };
static const GdkColor GREEN =  { 0, 0     , 0xffff, 0      };
static const GdkColor BLUE =   { 0, 0     , 0     , 0xffff };
static const GdkColor YELLOW = { 0, 0xffff, 0xffff, 0      };
static const GdkColor CYAN =   { 0, 0     , 0xffff, 0xffff };
static const GdkColor WHITE =  { 0, 0xffff, 0xffff, 0xffff };
static const GdkColor BLACK =  { 0, 0	  , 0	  , 0      };


/* disk_size: size in Bytes or in seconds */
/* is_time: whether disk_size represents time (TRUE) or Bytes (FALSE)*/
void
gb_cairo_fillbar_set_disk_size(GBCairoFillBar *bar,
							gdouble disk_size,
							gboolean is_time,
							gdouble tolerance_percent,
							gboolean allow_overburn)
{
	GB_LOG_FUNC
	g_return_if_fail (bar != NULL);
	bar->priv->disk_size = disk_size;
	bar->priv->is_time = is_time;
	if(tolerance_percent >= 1)
		bar->priv->tolerance_percent = tolerance_percent - 1;
	else
		bar->priv->tolerance_percent = 0;
	bar->priv->allow_overbun = allow_overburn;
	/*update*/
	gtk_widget_queue_draw (GTK_WIDGET (bar));
}


void
gb_cairo_fillbar_set_project_total_size(GBCairoFillBar *bar, gdouble project_total_size)
{
	GB_LOG_FUNC
	g_return_if_fail(bar != NULL);
	bar->priv->project_total_size = project_total_size;
	/*update*/
	gtk_widget_queue_draw (GTK_WIDGET (bar));
}


gdouble
gb_cairo_fillbar_get_disk_size(GBCairoFillBar *bar)
{
	GB_LOG_FUNC
	g_return_val_if_fail(bar != NULL, 0.0);
	return bar->priv->disk_size;
}


gboolean
gb_cairo_fillbar_get_is_time(GBCairoFillBar *bar)
{
	GB_LOG_FUNC
	g_return_val_if_fail(bar != NULL, FALSE);
	return bar->priv->is_time;
}


gdouble
gb_cairo_fillbar_get_project_total_size(GBCairoFillBar *bar)
{
	GB_LOG_FUNC
	g_return_val_if_fail(bar != NULL, 0.0);
	return bar->priv->project_total_size;
}


static void
rgb_to_hls (gdouble *r, gdouble *g, gdouble *b)
{
    GB_LOG_FUNC
	gdouble min, max, red, green, blue, h, l, s, delta;	
	red = *r;
	green = *g;
	blue = *b;

	if (red > green)
	{
		if (red > blue)
			max = red;
		else
			max = blue;
	
		if (green < blue)
			min = green;
		else
			min = blue;
	}
	else
	{
		if (green > blue)
			max = green;
		else
			max = blue;
	
		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min)
	{
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);
	
		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;
	
		h *= 60;
		if (h < 0.0)
			h += 360;
	}

	*r = h;
	*g = l;
	*b = s;
}


static void 
hls_to_rgb (gdouble *h, gdouble *l, gdouble *s)
{
    GB_LOG_FUNC
	gdouble hue, lightness, saturation, m1, m2, r, g, b;	
	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
		
	m1 = 2 * lightness - m2;

	if (saturation == 0)
	{
		*h = lightness;
		*l = lightness;
		*s = lightness;
	}
	else
	{
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;
	
		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;
	
		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;
	
		*h = r;
		*l = g;
		*s = b;
	}
}


static void
rotate_mirror_translate (cairo_t *cr, double radius, double x, double y,
			 gboolean mirror_horizontally, gboolean mirror_vertically)
{
    GB_LOG_FUNC	
	cairo_matrix_t matrix_rotate;
	cairo_matrix_t matrix_mirror;
	cairo_matrix_t matrix_result;
	
	double r_cos = cos(radius);
	double r_sin = sin(radius);
	
	cairo_matrix_init (&matrix_rotate, r_cos, r_sin, r_sin, r_cos, x, y);
	cairo_matrix_init (&matrix_mirror, mirror_horizontally ? -1 : 1, 0, 0, 
            mirror_vertically ? -1 : 1, 0, 0);
	cairo_matrix_multiply (&matrix_result, &matrix_mirror, &matrix_rotate);
	cairo_set_matrix (cr, &matrix_result);
}


static void
shade (const CairoColor *in, CairoColor *out, float k)
{
    GB_LOG_FUNC
	double red   = in->r;
	double green = in->g;
	double blue  = in->b;
	
	rgb_to_hls (&red, &green, &blue);
	
	green *= k;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;
	
	blue *= k;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;
	
	hls_to_rgb (&red, &green, &blue);
	
	out->r = red;
	out->g = green;
	out->b = blue;
	out->a = in->a;
}


static void
rounded_rectangle (cairo_t *cr,
					double x,
					double y,
					double width,
					double height,
                   	double radius,
					RectangleCorners draw_corners)
{
    GB_LOG_FUNC
	if (draw_corners & TOP_LEFT_CORNER)
		cairo_move_to (cr, x + radius, y);
	else
		cairo_move_to (cr, x, y);
	
	if (draw_corners & TOP_RIGHT_CORNER)
		cairo_arc (cr,
					x + width - radius, /* arc´s center x */
					y + radius,			/* arc´s center y */
					radius,
					M_PI * 1.5,			/* starting angle*/
					M_PI * 2 );			/* end angle */
			
	else
		cairo_line_to (cr, x + width, y);
	
	if (draw_corners & BOTTOM_RIGHT_CORNER)
		cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, M_PI * 0.5 );
	else
		cairo_line_to (cr, x + width, y + height);
	
	if (draw_corners & BOTTOM_LEFT_CORNER)
		cairo_arc (cr, x + radius, y + height - radius, radius, M_PI * 0.5, M_PI );
	else
		cairo_line_to (cr, x, y + height);
	
	/* close path */
	if (draw_corners & TOP_LEFT_CORNER)
		cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5 );
	else
		cairo_line_to (cr, x, y);
}


/* Sets the left edge of a GdkRectangle to x_pos.
 May change the width, but will never change the right edge of the rectangle. */
static void
rectangle_set_left(GdkRectangle *rect, gint x_pos)
{
    GB_LOG_FUNC
	gint old_width = rect->width;
	gint old_x = rect->x;
	rect->width -= ( x_pos - rect->x);
	rect->x = x_pos;
	
	/*do not allow negative width (x_pos to become the right edge)*/
	if(rect->width < 0)
	{
		rect->x = old_x + old_width;
		rect->width = 0;
	}
}


/* Sets the right edge of a GdkRectangle to x_pos.
 May change the width, but will never change the left edge of the rectangle. */
static void
rectangle_set_right(GdkRectangle *rect, gint x_pos)
{
    GB_LOG_FUNC
	rect->width = x_pos - rect->x;
	if(rect->width < 0)
		rect->width = 0;
}


static void	
gb_cairo_fillbar_draw_background (cairo_t *cr,
								const GtkStyle *style, 
								const GtkStateType state_type,
 								const GtkAllocation *widget_rect)
{
	GB_LOG_FUNC
	double height = widget_rect->height - style->ythickness*2;
	double width = widget_rect->width - style->xthickness*2;
	/*double y = widget_rect->y + style->ythickness;
	double x = widget_rect->x + style->xthickness;*/
	double y = style->ythickness;
	double x = style->xthickness;
	
	CairoColor  border_color = 
    {
        style->bg[GTK_STATE_NORMAL].red / 65535.0,
		style->bg[GTK_STATE_NORMAL].green / 65535.0,
		style->bg[GTK_STATE_NORMAL].blue / 65535.0
    };
    
	CairoColor border_color_shaded;									  
	shade(&border_color, &border_color_shaded, 0.5);
	cairo_pattern_t *pattern;
	cairo_save(cr);
	cairo_set_line_width (cr, 1.0);

	/* Fill with bg color */
	cairo_set_source_rgb (cr, style->bg[state_type].red/65535.0,
            style->bg[state_type].green/65535.0, style->bg[state_type].blue/65535.0);

	cairo_rectangle (cr, x, y, width, height);	
	cairo_fill (cr);

	/* Draw border */
	rounded_rectangle (cr, x + 0.5, y +0.5, width - 1, height - 1, 1.5, ALL_CORNERS );
	cairo_set_source_rgb (cr, border_color_shaded.r, border_color_shaded.g, border_color_shaded.b );
	cairo_stroke (cr);

	/* Top shadow */
	cairo_rectangle (cr, x + 1, y + 1, width - 2, 4);
	pattern = cairo_pattern_create_linear (x, y, x, y + 4);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.15);	
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0);	
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	/* Left shadow */
	cairo_rectangle (cr, x + 1, y + 1, 4, height - 2);
	pattern = cairo_pattern_create_linear (x, y, x+4, y);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.15);	
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0);	
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);
	cairo_restore(cr);
}


static void
gb_cairo_fillbar_fill_rect(cairo_t *cr,
							const GdkRectangle *rect,
							const CairoColor *fill_color)
{
    GB_LOG_FUNC	
	cairo_pattern_t *pattern;		
	CairoColor shade_color;
	
	cairo_set_line_width (cr, 1.0);
	
	/* Draw flat color*/
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_set_source_rgb(cr, fill_color->r, fill_color->g, fill_color->b);
	cairo_fill_preserve(cr);
	
	/*Draw gradient */
	shade(fill_color, &shade_color, 1.2); /*"amplifies" */
	pattern = cairo_pattern_create_linear (rect->x, rect->y, rect->x, rect->height);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, shade_color.r, shade_color.g,
            shade_color.b, shade_color.a);
	cairo_pattern_add_color_stop_rgba(pattern, /*this adds shininess (OMG, are we in kde? hehe)*/
            0.6, fill_color->r, fill_color->g, fill_color->b, fill_color->a); 
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, shade_color.r, shade_color.g,
            shade_color.b,shade_color.a);
	cairo_set_source(cr, pattern);
	cairo_fill(cr);
	cairo_pattern_destroy(pattern);	
}


static void
gb_cairo_fillbar_draw_fill (cairo_t *cr,
						const GtkStyle *style,
						const GtkAllocation *widget_rect,
						gdouble max_value,	   /*the 100% value*/
						gdouble max_disk_size, /*the disk size mark will be placed here*/
						gdouble current_size,   /*indicates the size to fill*/
						gdouble tolerance)	   /*the size to fill in yellow*/
{
	GB_LOG_FUNC
	/* FILL COLORS */
	const CairoColor FILL_GREEN  = {0.2, 0.7, 0.1, 1};
	const CairoColor FILL_YELLOW = {0.7, 0.7, 0.1, 1};
	const CairoColor FILL_RED = {0.7, 0.2, 0.1, 1};
	
	const CairoColor BORDER_GREEN = {0.5, 0.9, 0.3, 0.6};
	const CairoColor BORDER_YELLOW = {0.9,0.9,0.3, 0.6};
	const CairoColor BORDER_RED = {0.9, 0.5, 0.3, 0.6};
	
	const CairoColor END_BORDER_GREEN = {0.1, 0.6, 0.05, 1};
	const CairoColor END_BORDER_YELLOW = {0.5,0.5,0.05, 1};
	const CairoColor END_BORDER_RED = {0.6, 0.1, 0.05, 1};
	
	gint real_width  = widget_rect->width  - (style->xthickness + 1)*2;
	gint real_height = widget_rect->height - (style->ythickness + 1)*2;
	/*gint real_x = widget_rect->x + style->xthickness +1;
	gint real_y = widget_rect->y + style->ythickness +1;*/
	gint real_x = style->xthickness +1;
	gint real_y = style->ythickness +1;
	
	g_return_if_fail(max_value != 0);

	gdouble one = (gdouble)real_width / max_value;
	gdouble max_size_minus_tolerance = max_disk_size - tolerance;
	gdouble max_size_plus_tolerance = max_disk_size + tolerance;
	
	GdkRectangle total_rect;
	
	total_rect.height = real_height;
	total_rect.width  = ceil(current_size*one);
	total_rect.x = real_x;
	total_rect.y = real_y;
    
    /*this should not happen!*/
    if(total_rect.width > real_width)
        total_rect.width = real_width;

	GdkRectangle green_rect = total_rect;
	GdkRectangle warning_rect = total_rect;
	GdkRectangle oversize_rect = total_rect;
	gboolean paint_warning = FALSE, paint_oversize = FALSE;
	
	if(current_size > max_size_minus_tolerance)
	{
		green_rect.height = real_height;
		green_rect.width = (gint)(max_size_minus_tolerance *one);
		green_rect.x = real_x;
		green_rect.y = real_y;
		
		rectangle_set_left( &warning_rect, real_x + (gint)(max_size_minus_tolerance*one) );		
		paint_warning = TRUE;
		
		if(current_size > max_size_plus_tolerance )
		{			
			rectangle_set_right( &warning_rect, real_x + (gint)(max_size_plus_tolerance*one) );
			rectangle_set_left( &oversize_rect, real_x + (gint)(max_size_plus_tolerance*one) );			
			paint_oversize = TRUE;
		}
	}
	
	cairo_save(cr);
	cairo_rectangle (cr, real_x, real_y, real_width, real_height);
	cairo_clip (cr);
	cairo_new_path (cr);
	
	if(paint_oversize)
	{
		gb_cairo_fillbar_fill_rect(cr, &oversize_rect, &FILL_RED);
		cairo_set_source_rgba(cr, BORDER_RED.r, BORDER_RED.g, BORDER_RED.b, BORDER_RED.a);
		cairo_move_to(cr, oversize_rect.x, oversize_rect.y + 0.5);			
		cairo_line_to(cr, oversize_rect.x + oversize_rect.width - 0.5, oversize_rect.y + 0.5);			
		cairo_line_to(cr, oversize_rect.x + oversize_rect.width  - 0.5, oversize_rect.y + oversize_rect.height - 0.5);			
		cairo_line_to(cr, oversize_rect.x, oversize_rect.y + oversize_rect.height - 0.5);			
		cairo_stroke(cr);			
	}
    
	if(paint_warning)
	{
		gb_cairo_fillbar_fill_rect(cr, &warning_rect, &FILL_YELLOW);
		cairo_set_source_rgba(cr, BORDER_YELLOW.r, BORDER_YELLOW.g, BORDER_YELLOW.b, BORDER_YELLOW.a);
		cairo_move_to(cr, warning_rect.x, warning_rect.y + 0.5);			
		cairo_line_to(cr, warning_rect.x + warning_rect.width - 0.5, warning_rect.y + 0.5);				
		if(paint_oversize)
		{
			cairo_stroke(cr);
			cairo_move_to(cr, warning_rect.x + warning_rect.width  - 0.5, warning_rect.y + warning_rect.height - 0.5);
		}
		else
		{
			cairo_line_to(cr, warning_rect.x + warning_rect.width  - 0.5, warning_rect.y + warning_rect.height - 0.5);
		}				
		cairo_line_to(cr, warning_rect.x, warning_rect.y + warning_rect.height - 0.5);			
		cairo_stroke(cr);	
	}

	gb_cairo_fillbar_fill_rect(cr, &green_rect, &FILL_GREEN);
	
	cairo_set_source_rgba(cr, BORDER_GREEN.r, BORDER_GREEN.g, BORDER_GREEN.b, BORDER_GREEN.a);
		
	cairo_move_to(cr, green_rect.x + green_rect.width - 0.5, green_rect.y + 0.5);			
	cairo_line_to(cr, green_rect.x + 0.5, green_rect.y + 0.5);			
	cairo_line_to(cr, green_rect.x + 0.5, green_rect.y + green_rect.height - 0.5);			
	cairo_line_to(cr, green_rect.x + green_rect.width  - 0.5, green_rect.y + green_rect.height - 0.5);
	if(!paint_warning)
		cairo_line_to(cr, green_rect.x + green_rect.width - 0.5, green_rect.y + 0.5);
	cairo_stroke(cr);
	
	/*changes the coordinate origin (and mirrors)*/
	rotate_mirror_translate (cr, 0, real_x, real_y, FALSE, FALSE);
	
	cairo_save(cr);
	cairo_rectangle (cr, 0, 0, total_rect.width + 4, total_rect.height);		
	cairo_clip (cr);

	/* Right shadow */
	cairo_pattern_t *pattern;

	cairo_translate (cr, total_rect.width + 1, 0);
	pattern = cairo_pattern_create_linear (0, 0, 3, 0);
	cairo_pattern_add_color_stop_rgba (pattern, 0., 0., 0., 0., 0.15);	
	cairo_pattern_add_color_stop_rgba (pattern, 1., 0., 0., 0., 0);	
	cairo_rectangle (cr,0, 0, 3,total_rect.height);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	/* End of bar line */
	cairo_move_to (cr, -0.5, 0);
	cairo_line_to (cr, -0.5, total_rect.height);
	if(paint_oversize)
		cairo_set_source_rgba(cr,  END_BORDER_RED.r, END_BORDER_RED.g, END_BORDER_RED.b, END_BORDER_RED.a);
	else if(paint_warning)
		cairo_set_source_rgba(cr, END_BORDER_YELLOW.r, END_BORDER_YELLOW.g, END_BORDER_YELLOW.b, END_BORDER_YELLOW.a);
	else
		cairo_set_source_rgba(cr, END_BORDER_GREEN.r, END_BORDER_GREEN.g, END_BORDER_GREEN.b, END_BORDER_GREEN.a);
	
	cairo_stroke (cr);
	cairo_restore(cr);
	
	/* maximum line											*/
	
	/* it seems that adding the 0.5 offset we can avoid	blurred lines:		*/
	/* http://lists.freedesktop.org/archives/cairo/2005-July/004566.html 	*/
	
	cairo_set_line_width (cr, 1.0);
	cairo_move_to(cr, ((gint)( one * max_disk_size ) + 0.5), real_height);
	cairo_line_to(cr, ((gint)( one * max_disk_size ) + 0.5), real_height/2);
	cairo_set_source_rgba (cr, 0.3,0.3,0.3,1);
	cairo_stroke(cr);
	
	/* cairo_set_line_width (cr, 2.0);
	   cairo_move_to(cr, ((int)( one * max_disk_size )) + 2, real_height);
	   cairo_line_to(cr, ((int)( one * max_disk_size )) + 2, real_height/2);
	   cairo_set_source_rgba (cr, 0.3,0.3,0.3,0.2);
	   cairo_stroke(cr);*/
	cairo_restore(cr);
}


/*the returned string should be freed with g_free()*/
/*the text is formated as pango markup*/
gchar*
gb_cairo_fillbar_get_current_text(GBCairoFillBar *bar)
{
    GB_LOG_FUNC
	if(bar->priv->is_time)
	{
		guint secs = (bar->priv->project_total_size)%60;
  		guint mins  = (bar->priv->project_total_size - secs)/60;
		guint secs_remaining = (bar->priv->disk_size - bar->priv->project_total_size)%60;
        guint mins_remaining = (bar->priv->disk_size - bar->priv->project_total_size - secs_remaining)/60;
		return g_strdup_printf(_("%d mins %d secs used - %d mins %d secs remaining"),
            mins, secs, mins_remaining, secs_remaining);
	}
	else
	{		        
    	gchar *current = NULL, *remaining = NULL, *buf = NULL;
    	if(bar->priv->allow_overbun &&
			(bar->priv->project_total_size > bar->priv->disk_size) &&
			(bar->priv->project_total_size < ( bar->priv->disk_size *( 1 + bar->priv->tolerance_percent))))
    	{
        	current = gbcommon_humanreadable_filesize( bar->priv->project_total_size );
        	remaining = gbcommon_humanreadable_filesize( bar->priv->disk_size );
        	buf = g_strdup_printf(_("%s of %s used. <b>Overburning</b>."), current, remaining);
    	}
    	else if(bar->priv->project_total_size < bar->priv->disk_size)
    	{
        	current = gbcommon_humanreadable_filesize( bar->priv->project_total_size );
        	remaining = gbcommon_humanreadable_filesize( bar->priv->disk_size - bar->priv->project_total_size );
       		buf = g_strdup_printf(_("%s used - %s remaining."), current, remaining);
   		}
   		else
   	 	{
        	current = gbcommon_humanreadable_filesize( bar->priv->project_total_size );
        	remaining = gbcommon_humanreadable_filesize((bar->priv->disk_size));
        	buf = g_strdup_printf(_("%s of %s used. <b>Disk full</b>."), current, remaining);
    	}
    	g_free(current);
    	g_free(remaining);
    	return buf;
	}
}


/*render the text using Pango*/
static void
gb_cairo_fillbar_paint_text(cairo_t *cr, GBCairoFillBar *bar,
							gint amount)
{
	GB_LOG_FUNC
	GtkWidget *widget = GTK_WIDGET (bar);	
  	gchar *text_buf = gb_cairo_fillbar_get_current_text (bar);  
  	
    PangoLayout *layout = gtk_widget_create_pango_layout (widget, text_buf);
	pango_layout_set_markup(layout,text_buf,-1);
  	pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  	pango_layout_set_width (layout, widget->allocation.width * PANGO_SCALE);
    PangoRectangle logical_rect;
  	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  	/*center the text*/
  	gint x = widget->style->xthickness + 1 +
		(widget->allocation.width - 2 * widget->style->xthickness - 2 - logical_rect.width)*0.5; 
    gint y = widget->style->ythickness + 1 +
		(widget->allocation.height - 2 * widget->style->ythickness - 2 - logical_rect.height)*0.5;

	/*cliping rects*/
	/*this cool thing that changes the text colour when the bar is below */
    GdkRectangle rect;  
  	rect.x = widget->style->xthickness;
  	rect.y = widget->style->ythickness;
  	rect.width = widget->allocation.width - 2 * widget->style->xthickness;
  	rect.height = widget->allocation.height - 2 * widget->style->ythickness;

    GdkRectangle normal_clip;
	GdkRectangle prelight_clip = normal_clip = rect;

  	prelight_clip.width = amount;
  	normal_clip.x += amount;
  	normal_clip.width -= amount;

 	/*Uncomment this in order to see the clipping regions*/
/*	
	cairo_rectangle (cr, prelight_clip.x , prelight_clip.y , prelight_clip.width , prelight_clip.height);
	cairo_stroke (cr);	
	cairo_rectangle (cr, normal_clip.x , normal_clip.y , normal_clip.width , normal_clip.height);
	cairo_stroke (cr);
 */   

	/*TODO: should we change the detail ("progressbar")?*/
	gtk_paint_layout(widget->style, widget->window, GTK_STATE_PRELIGHT, FALSE, &prelight_clip,
            widget, "progressbar", x, y, layout);  
	gtk_paint_layout(widget->style,	widget->window, GTK_STATE_NORMAL, FALSE, &normal_clip,
            widget, "progressbar", x, y, layout);
    g_object_unref (layout);
    g_free (text_buf);
}


static gboolean
gb_cairo_fillbar_expose(GtkWidget *widget, GdkEventExpose *event)
{
	GB_LOG_FUNC
	cairo_t *cr = gdk_cairo_create(widget->window);  /*Get a cairo context*/
    g_return_val_if_fail(cr != NULL, FALSE);
    
	cairo_rectangle(cr,						/* set a clip region for the expose event. 		*/	 
	                event->area.x,			/* This way we only draw the required region.	*/
					event->area.y,
                    event->area.width,
					event->area.height);	
	cairo_clip(cr);

	/*draw the widget´s background*/
	gb_cairo_fillbar_draw_background(cr, widget->style, GTK_WIDGET_STATE(widget), &widget->allocation);
	GBCairoFillBar *bar = GB_CAIRO_FILLBAR(widget);
	gdouble max_value = 0.0, max_disk_size = 0.0, current_size = 0.0, tolerance = 0.0, bar_oversize = 0.0; 
	if(bar->priv->is_time)
	{
		current_size = ((gdouble)bar->priv->project_total_size)/60.0;
		max_disk_size =((gdouble)bar->priv->disk_size)/60.0;
		tolerance = bar->priv->tolerance_percent * max_disk_size;
        bar_oversize = 10.0;
        if(bar_oversize < tolerance)
           bar_oversize = tolerance;
        max_value = (((max_disk_size + bar_oversize) > current_size) ? (max_disk_size + bar_oversize) : current_size);
	}
	else
	{
		current_size = ((gdouble)bar->priv->project_total_size)/1024.0/1024.0;
		max_disk_size =((gdouble)bar->priv->disk_size)/1024.0/1024.0;
		tolerance = bar->priv->tolerance_percent * max_disk_size;
        bar_oversize = 100.0;
        if(bar_oversize<tolerance)
            bar_oversize = tolerance;
        max_value = (((max_disk_size + bar_oversize) > current_size) ? (max_disk_size + bar_oversize) : current_size);
	}
	
	/*fill the bar*/
	if(max_disk_size > 0.0)
	{
		if(current_size > 0.0)
			gb_cairo_fillbar_draw_fill(cr, widget->style, &widget->allocation, max_value,
                    max_disk_size, current_size, tolerance);
        /*paint text*/
        gint amount = (widget->allocation.width - 2 * widget->style->xthickness) *
				(gdouble)current_size/(gdouble)max_value;	
        gb_cairo_fillbar_paint_text(cr, bar, amount);
	}	
	cairo_destroy(cr);	
		
	return FALSE;
}


static void
gb_cairo_fillbar_class_init(GBCairoFillBarClass *klass)
{
    GB_LOG_FUNC
    g_type_class_add_private(klass, sizeof(GBCairoFillBarPrivate));
    GTK_WIDGET_CLASS(klass)->expose_event = gb_cairo_fillbar_expose;
}


static void
gb_cairo_fillbar_init(GBCairoFillBar *bar)
{
    GB_LOG_FUNC
    bar->priv = GB_CAIRO_FILLBAR_GET_PRIVATE(bar);
    bar->priv->disk_size = 0;
    bar->priv->is_time = FALSE;
    bar->priv->project_total_size = 0;
    bar->priv->allow_overbun = TRUE;
    bar->priv->tolerance_percent = 0.02;    
    
    GtkRcStyle *rc_style = gtk_widget_get_modifier_style(GTK_WIDGET(bar));

    /*rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG;*/
    rc_style->color_flags[GTK_STATE_PRELIGHT] = GTK_RC_FG;
    /*rc_style->color_flags[GTK_STATE_ACTIVE] = GTK_RC_FG;*/
    /*rc_style->fg[GTK_STATE_NORMAL] = BLACK;*/
    rc_style->fg[GTK_STATE_PRELIGHT] = WHITE;
    /*rc_style->fg[GTK_STATE_ACTIVE] = RED;*/

    gtk_widget_modify_style(GTK_WIDGET(bar), rc_style);
    
    /*g_object_unref (rc_style);*/ /*only necesary if created with gtk_rc_style_new ()*/
    
}


GtkWidget*
gb_cairo_fillbar_new (void)
{
    GB_LOG_FUNC
    return g_object_new(GB_TYPE_CAIRO_FILLBAR, NULL);
}

