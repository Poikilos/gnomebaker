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
 * File: mkisofs.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Sun Jan 11 17:23:09 2004
 */

#include "mkisofs.h"
#include "gnomebaker.h"
#include "preferences.h"
#include "progressdlg.h"
#include "datacd.h"
#include "gbcommon.h"
#include "devices.h"

/*
	mkisofs -o gb.iso -R -J -hfs *

 mkisofs -V "Jamie Michelle Wedding" -p "Luke Biddell" -A "GnomeBaker" -o gb.iso -R -J -hfs *

 also 
 -L (include dot files) -X (mixed case filenames) -s (multiple dots in name)
	 
shell> NEXT_TRACK=`cdrecord -msinfo dev=0,6,0`
shell> echo $NEXT_TRACK
shell> mkisofs -R -o cd_image2 -C $NEXT_TRACK -M /dev/scd5 private_collection/
*/


void
mkisofs_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("<b>Creating data disk image...</b>"));
	progressdlg_increment_exec_number();
	
	gchar* file = preferences_get_create_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_OK,
			_("A data CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?"));
		gdk_flush();
		gdk_threads_leave();
		
		ExecCmd* e = (ExecCmd*)ex;
		
		if(ret  == GTK_RESPONSE_YES)		
			e->state = SKIP;
		else if(ret == GTK_RESPONSE_CANCEL)
			e->state = CANCELLED;
	}
	
	g_free(file);
}


void
mkisofs_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
	
	const gchar* percent = strrchr(buffer, '%');
	if(percent != NULL)
	{
		const gchar* ptr = percent;
		while((!g_ascii_isspace(*ptr)) && (ptr > (gchar*)buffer))
			--ptr;
		
		ptr++;
		
		gchar* pct = g_malloc((percent - ptr + 1) * sizeof(gchar));
		memset(pct, 0x0, (percent - ptr + 1) * sizeof(gchar));
		strncpy(pct, ptr, percent - ptr);
		
		progressdlg_set_fraction(atof(pct)/100.0);
		g_free(pct);
	}
	
	progressdlg_append_output(buffer);
}


gboolean
mkisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
	GB_LOG_FUNC
	gchar *icon, *file, *filepath;
	glong size;
		
	gtk_tree_model_get (model, iter, DATACD_COL_ICON, &icon, DATACD_COL_FILE, &file,
		DATACD_COL_SIZE, &size, DATACD_COL_PATH, &filepath, -1);
	
	/* Only add files that are not part of an existing session */
	if(g_ascii_strcasecmp(icon, DATACD_EXISTING_SESSION_ICON) != 0)
	{
		g_message( "%s %ld %s", file, size, filepath);
		
		gchar* buffer = g_strdup_printf("%s=%s", file, filepath);		
		exec_cmd_add_arg((ExecCmd*)user_data, "%s", buffer);	
		g_free(buffer);
	}
	g_free(icon);	
	g_free(file);	
	g_free(filepath);
	
	return FALSE; /* do not stop walking the store, call us with next row */
}


gboolean
mkisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, const gchar* iso)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, FALSE);
	g_return_val_if_fail(datamodel != NULL, FALSE);
	g_return_val_if_fail(iso != NULL, FALSE);
	
	GladeXML* dialog = glade_xml_new(glade_file, widget_isofsdlg, NULL);
	
	GtkEntry* created = GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_createdby));
	gtk_entry_set_text(created, g_get_real_name());
	
	GtkWidget* dlg = glade_xml_get_widget(dialog, widget_isofsdlg);
	gint ret = gtk_dialog_run(GTK_DIALOG(dlg));
	if(ret == GTK_RESPONSE_OK)
	{
		exec_cmd_add_arg(e, "%s", "mkisofs");		
		exec_cmd_add_arg(e, "-V \"%s\"", gtk_entry_get_text(
			GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_volume))));
		
		exec_cmd_add_arg(e, "-p \"%s\"", gtk_entry_get_text(created));	
	/* 	exec_cmd_add_arg(e, "-A \"%s\"", "GnomeBaker");  */
				
		exec_cmd_add_arg(e, "%s", "-r");
		exec_cmd_add_arg(e, "%s", "-f");
		exec_cmd_add_arg(e, "%s", "-J");
		/*exec_cmd_add_arg(e, "%s", "-hfs");*/
		exec_cmd_add_arg(e, "%s", "-gui");
		exec_cmd_add_arg(e, "%s", "-joliet-long");
		
		gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
		if(msinfo != NULL)
		{
			exec_cmd_add_arg(e, "-C %s", msinfo);
			
			gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
			exec_cmd_add_arg(e, "-M %s", writer);
			g_free(writer);			
			g_free(msinfo);
		}		
		
		exec_cmd_add_arg(e, "-o%s", iso);
		/*exec_cmd_add_arg(e, "-o%s", "test.iso");*/
			
		exec_cmd_add_arg(e, "%s", "-graft-points");
		gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);	
				
		e->preProc = mkisofs_pre_proc;
		e->readProc = mkisofs_read_proc;
	}
	
	gtk_widget_hide(dlg);
	gtk_widget_destroy(dlg);
	
	return (ret == GTK_RESPONSE_OK);
}
