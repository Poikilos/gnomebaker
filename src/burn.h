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
 * File: burn.h
 * Created by: luke <luke@dhcp-45-369>
 * Created on: Tue Jan 28 22:12:09 2003
 */

#ifndef _BURN_H_
#define _BURN_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <pthread.h>
#include <gnome.h>


gboolean burn_init ();
gboolean burn_iso (const gchar * const file);
gboolean burn_create_data_cd (GtkTreeModel* datamodel);
gboolean burn_create_audio_cd (GtkTreeModel* audiomodel);
gboolean burn_copy_data_cd ();
gboolean burn_copy_audio_cd ();
gboolean burn_blank_cdrw ();
gboolean burn_end_process ();
gboolean burn_create_data_dvd (GtkTreeModel* datamodel);
gboolean burn_format_dvdrw();

typedef enum 
{
	blank_cdrw = 0,
	copy_data_cd,
	copy_audio_cd,
	burn_cd_image,
	create_data_cd,
	create_audio_cd,
	create_mixed_cd,
	create_video_cd,
	format_dvdrw,
	create_data_dvd,
	BurnTypeCount
} BurnType;

static const gchar* const BurnTypeText[BurnTypeCount] = 
{
	"Blank CDRW",
	"Copy Data CD",
	"Copy Audio CD",
	"Burn CD Image",
	"Create Data CD",
	"Create Audio CD",
	"Create Mixed CD",
	"Create Video CD",
	"Format DVD+RW",
	"Create Data DVD"
};


#endif	/*_BURN_H_*/