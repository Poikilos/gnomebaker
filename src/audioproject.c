#include "audioproject.h"
#include "gbcommon.h"
#include "media.h"
#include "preferences.h"
#include "gnomebaker.h"

G_DEFINE_TYPE(AudioProject, audioproject, PROJECT_TYPE_WIDGET);


static DiskSize audio_disk_sizes[] = 
{
    {21.0, "21 min. CD"},
    {63.0, "63 min. CD"},
    {74.0, "74 min. CD"},
    {80.0, "80 min. CD"},
    {90.0, "90 min. CD"},
    {99.0, "99 min. CD"}
};


enum
{
    TARGET_URI_LIST,
    TARGET_STRING,
    TARGET_COUNT
};


enum
{
    AUDIO_COL_ICON = 0,
    AUDIO_COL_FILE,
    AUDIO_COL_DURATION,
    /*AUDIO_COL_SIZE,*/
    AUDIO_COL_ARTIST,
    AUDIO_COL_ALBUM,
    AUDIO_COL_TITLE,
    AUDIO_COL_INFO,
    AUDIO_NUM_COLS
};


static GtkTargetEntry target_entries[] = 
{
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"text/plain", 0, TARGET_STRING}
};


static void 
audioproject_move_selected(AudioProject* audio_project, const gboolean up)
{
    GB_LOG_FUNC
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(audio_project->tree);
    GtkTreeModel *model = gtk_tree_view_get_model(audio_project->tree);
    GList *list = gtk_tree_selection_get_selected_rows(selection, &model);    
    GList *current = NULL; 
    for((up ? (current = list) : (current = g_list_last(list))); 
        current != NULL; 
        (up ? (current = current->next) : (current = current->prev)))
    {
        GtkTreeIter from, to;
        GtkTreePath *path = (GtkTreePath*)current->data;
        gtk_tree_model_get_iter(model, &from, path);
        
        if(up) gtk_tree_path_prev(path);
        else gtk_tree_path_next(path);
            
        if(gtk_tree_model_get_iter(model, &to, path))
            gtk_list_store_swap(GTK_LIST_STORE(model), &from, &to);
            
        gtk_tree_path_free(path);     
    }
    
    g_list_free (list);        
}


// FIXME - This method needs to die and the variable maintained by the option menu selection callback
static gint 
audioproject_get_audioproject_size(AudioProject* audio_project)
{
    GB_LOG_FUNC    
    audio_project->selected_size = audio_disk_sizes[gtk_option_menu_get_history(PROJECT_WIDGET(audio_project)->menu)].size;   
    return audio_project->selected_size;
}


static gchar*
audioproject_format_progress_text(AudioProject *audio_project)
{
    GB_LOG_FUNC
    g_return_val_if_fail(audio_project->compilation_seconds < (audio_project->selected_size * 60), NULL);
    
    gint ss1 = ((gint)audio_project->compilation_seconds)%60;
    gint m1 = (((gint)audio_project->compilation_seconds)-ss1)/60;
    gint ss2 = ((gint)((audio_project->selected_size * 60) - audio_project->compilation_seconds))%60;
    gint m2 = (((gint)((audio_project->selected_size * 60) - audio_project->compilation_seconds))-ss2)/60;        
    return g_strdup_printf(_("%d mins %d secs used - %d mins %d secs remaining"), m1, ss1, m2, ss2);    
}   


static gboolean 
audioproject_update_progress_bar(AudioProject *audio_project, gboolean add, gdouble seconds)
{
    GB_LOG_FUNC
    gboolean ok = TRUE;
    
    GtkProgressBar *progress_bar = PROJECT_WIDGET(audio_project)->progress_bar;
    const gdouble disk_size = audioproject_get_audioproject_size(audio_project) * 60;
    
    if(add)
    {
        if((audio_project->compilation_seconds +  seconds) <= disk_size)
        {
            audio_project->compilation_seconds += seconds;
        }
        else /*do not add*/
        {
            gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
            _("Track is too large to fit in the remaining space on the CD"));
            ok = FALSE;
        }
    }
    else
    {
        audio_project->compilation_seconds -= seconds;
    }   
    
    if (audio_project->compilation_seconds < 0.0 || audio_project->compilation_seconds == 0.0)
    {
        audio_project->compilation_seconds = 0;
        gtk_progress_bar_set_fraction(progress_bar, 0.0);
        gchar *buf = audioproject_format_progress_text(audio_project);
        gtk_progress_bar_set_text(progress_bar, buf);
        g_free(buf);
        
        /* disable the create button as there's nothing on the disk */
        gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), FALSE);
    }   
    else
    {
        gdouble fraction = 0.0;
    
        if(disk_size > 0)
            fraction = (gdouble)audio_project->compilation_seconds/disk_size;
            
        if(audio_project->compilation_seconds > disk_size)
        {
            gtk_progress_bar_set_fraction(progress_bar, 1.0);
            gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), FALSE);
        }
        else
        {
            gtk_progress_bar_set_fraction(progress_bar, fraction);
            gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), TRUE);
        }
    
        gchar *buf = audioproject_format_progress_text(audio_project);
        gtk_progress_bar_set_text(progress_bar, buf);
        g_free(buf);
    }
    
    return ok;
}


static void
audioproject_foreach_fileselection(GtkTreeModel *file_model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(file_model != NULL);
    g_return_if_fail(iter != NULL);
    
    GList **rowref_list = (GList**)user_data;   
    GtkTreeRowReference *ref = gtk_tree_row_reference_new(file_model, path);
    if(ref != NULL)
        *rowref_list = g_list_append(*rowref_list, ref);
}


static void 
audioproject_on_open(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);

    GtkTreeView *view = (GtkTreeView*)user_data;    
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    gtk_tree_selection_selected_foreach(selection, 
        (GtkTreeSelectionForeachFunc)gbcommon_get_first_selected_row, &iter);   
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, AUDIO_COL_FILE, &file, -1);
    gbcommon_launch_app_for_file(file);
    g_free(file);
}


static void 
audioproject_on_move_up(gpointer widget, AudioProject *audio_project)
{
    GB_LOG_FUNC
    audioproject_move_selected(audio_project, TRUE);
}


static void 
audioproject_move_selected_up(Project *project)
{
    GB_LOG_FUNC
    audioproject_move_selected(AUDIOPROJECT_WIDGET(project), TRUE);
}


static void 
audioproject_on_move_down(gpointer widget, AudioProject *audio_project)
{
    GB_LOG_FUNC
    audioproject_move_selected(audio_project, FALSE);
}


static void 
audioproject_move_selected_down(Project *project)
{
    GB_LOG_FUNC
    audioproject_move_selected(AUDIOPROJECT_WIDGET(project), FALSE);
}


static gboolean
audioproject_add_file(AudioProject *audio_project, const gchar *file_name)
{
    GB_LOG_FUNC
    g_return_val_if_fail(file_name != NULL, FALSE);
    g_return_val_if_fail(audio_project != NULL, FALSE);
    
    GB_TRACE("audioproject_add_file - [%s]\n", file_name);
    /* ret is set to true as we want to ignore non audio files. */
    gboolean ret = TRUE;
    MediaInfo *info = media_info_new(file_name);
    if(info != NULL)
    {     
        switch(info->status)
        {
            case INSTALLED:
            {
                if(audioproject_update_progress_bar(audio_project, TRUE, (gdouble)info->duration))
                {
                    GdkPixbuf *icon = gbcommon_get_icon_for_mime(info->mime_type, 16);
            
                    GB_DECLARE_STRUCT(GtkTreeIter, iter);      
                    GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(audio_project->tree));
                    gtk_list_store_append(model, &iter);
                    gtk_list_store_set(
                        model, &iter, 
                        AUDIO_COL_ICON, icon, 
                        AUDIO_COL_FILE, (gchar*)file_name, 
                        AUDIO_COL_DURATION, info->formatted_duration->str,
                        /*AUDIO_COL_SIZE, info->filesize,*/
                        AUDIO_COL_ARTIST, info->artist->str, 
                        AUDIO_COL_ALBUM, info->album->str,
                        AUDIO_COL_TITLE, info->title->str, 
                        AUDIO_COL_INFO, info,
                        -1);
                
                    g_object_unref(icon);
                    
                }
                else
                {
                    media_info_delete(info);
                    ret = FALSE;
                }
                break;
            }
            default:
            {
                gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE, info->error->message);
                media_info_delete(info);
            }
        }
    }
    return ret;
}


static gboolean
audioproject_import_playlist_file(AudioProject *audio_project, const gchar *play_list_dir, const gchar *file)
{
    GB_LOG_FUNC
    g_return_val_if_fail(file != NULL, FALSE);
    g_return_val_if_fail(audio_project != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *file_name = NULL;
    if(!g_path_is_absolute(file)) /* This is a relative file, relative to the play_list */
        file_name = g_build_filename(play_list_dir, file, NULL);
    else 
        file_name = g_strdup(file);
    
    if(g_file_test(file_name, G_FILE_TEST_EXISTS))
        ret = audioproject_add_file(audio_project, file_name);
    else 
    {
        gchar *message = g_strdup_printf(_("The file [%s] referred to in the play_list could not be imported as it does not exist."), file_name);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
        g_free(message);                
    }
    g_free(file_name);
    return ret;
}


static gboolean
audioproject_import_m3u_playlist(AudioProject *audio_project, const gchar *m3u_file)
{
    GB_LOG_FUNC
    g_return_val_if_fail(m3u_file != NULL, FALSE);
    g_return_val_if_fail(audio_project != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *m3u_dir = g_path_get_dirname(m3u_file);
    gchar **lines = gbcommon_get_file_as_list(m3u_file);
    gchar **line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar *file = *line;
        g_strstrip(file);
        GB_TRACE("audioproject_import_m3u_playlist - [%s]\n", file);
        if((strlen(file) > 0) && (file[0] != '#') && !g_ascii_isspace(file[0]))
            ret = audioproject_import_playlist_file(audio_project, m3u_dir, file);
        ++line;
    }
    g_free(m3u_dir);
    g_strfreev(lines);
    return ret;
}


static gboolean 
audioproject_import_pls_playlist(AudioProject *audio_project, const gchar *pls_file)
{
    GB_LOG_FUNC
    g_return_val_if_fail(pls_file != NULL, FALSE);
    g_return_val_if_fail(audio_project != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *pls_dir = g_path_get_dirname(pls_file);
    gchar **lines = gbcommon_get_file_as_list(pls_file);
    gchar **line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar *entry = *line;
        g_strstrip(entry);        
        if((entry[0] != '[') && (strstr(entry, "File") != NULL))
        {
            const gchar *file = strchr(entry, '=');
            if(file != NULL)
            {
                ++file;            
                GB_TRACE("audioproject_import_pls_playlist - [%s]\n", file);
                ret = audioproject_import_playlist_file(audio_project, pls_dir, file);
            }
            else
            {
                g_warning("audioproject_import_pls_playlist - file [%s] contains invalid line [%s]", pls_file, entry);   
            }
        }
        ++line;
    }
    g_free(pls_dir);
    g_strfreev(lines);
    return ret;
}


static gboolean 
audioproject_import_supported_playlist(AudioProject *audio_project, const gchar *mime, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(mime != NULL, FALSE);
    g_return_val_if_fail(play_list != NULL, FALSE);
        
    GtkTreeView *view = audio_project->tree;
    GtkTreeModel *model = gtk_tree_view_get_model(view);    
    gboolean ret = FALSE;
    if(g_ascii_strcasecmp(mime, "audio/x-mpegurl") == 0)
        ret = audioproject_import_m3u_playlist(audio_project, play_list);
    else if(g_ascii_strcasecmp(mime, "audio/x-scpls") == 0)      
        ret = audioproject_import_pls_playlist(audio_project, play_list);
    return ret;
}


static void 
audioproject_on_list_dbl_click(GtkTreeView *tree_view, GtkTreePath *path,
                        GtkTreeViewColumn *col, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(tree_view != NULL);
    g_return_if_fail(path != NULL);
    
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    g_return_if_fail(model != NULL);
    
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    gtk_tree_model_get_iter(model, &iter, path);
    
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, AUDIO_COL_FILE, &file, -1);
    gbcommon_launch_app_for_file(file);
    g_free(file);
}


void /* libglade callback */
audioproject_on_audioproject_size_changed(GtkOptionMenu *option_menu, AudioProject *audio_project)
{
    GB_LOG_FUNC
        
    GtkProgressBar *progress_bar = PROJECT_WIDGET(audio_project)->progress_bar;
    g_return_if_fail(progress_bar != NULL);
    
    audio_project->selected_size = audioproject_get_audioproject_size(audio_project);     
    gdouble fraction  = (audio_project->compilation_seconds)/(audio_project->selected_size*60);

    if(fraction>1.0)
    {
         gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
            _("Not enough space on the disk"));
        
        /* disable the create button*/
        gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), FALSE);
        gtk_progress_bar_set_fraction(progress_bar, 1.0); 

    }
    else
    {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
        gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), TRUE);
    }
    
    gchar *buf = audioproject_format_progress_text(audio_project);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
    preferences_set_int(GB_AUDIO_DISK_SIZE, gtk_option_menu_get_history(option_menu));
    g_free(buf);
    
}


static gboolean
audioproject_foreach_func(GtkTreeModel *audio_model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            GSList **infos)
{
    MediaInfo *info = NULL;
    gtk_tree_model_get (audio_model, iter, AUDIO_COL_INFO, &info, -1);
    *infos = g_slist_append(*infos, info);
    return FALSE; /*do not stop walking the store, call us with next row*/
}


void /* libglade callback */
audioproject_on_create_audiocd(gpointer widget, AudioProject *audio_project)
{
    GB_LOG_FUNC
    
    /* Here we should get a glist of files to burn to the cd or something like that */
    GtkTreeModel *audio_model = gtk_tree_view_get_model(audio_project->tree);
    g_return_if_fail(audio_model != NULL);        
    
    GSList *infos = NULL;
    gtk_tree_model_foreach(audio_model, (GtkTreeModelForeachFunc)audioproject_foreach_func, &infos);    
    burn_create_audio_cd(infos);
    /* NO free here for the GSList data elements as we don't own the MediaInfo structures */
    g_slist_free(infos);
}


gboolean 
audioproject_is_supported_playlist(const gchar *mime)
{
    GB_LOG_FUNC
    g_return_val_if_fail(mime != NULL, FALSE);           
    
    if((g_ascii_strcasecmp(mime, "audio/x-mpegurl") == 0) || 
            (g_ascii_strcasecmp(mime, "audio/x-scpls") == 0))
        return TRUE;
    return FALSE;
}


gboolean 
audioproject_import_playlist(AudioProject *audio_project, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);
    
    gboolean ret = FALSE;
    gchar *mime = gbcommon_get_mime_type(play_list);
    if(audioproject_is_supported_playlist(mime))
        ret = audioproject_import_supported_playlist(audio_project, mime, play_list);
    else
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
          _("The file you have selected is not a supported play_list. Please select a pls or m3u file."));
    g_free(mime);
    return ret;
}


static gboolean
audioproject_export_m3u(AudioProject *audio_project, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    FILE *file = NULL;   
    if ((file = fopen(play_list, "w")) == 0)
    {
        g_warning("audioproject_export_m3u - Failed to write play_list [%s]", play_list);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            GTK_BUTTONS_NONE, _("Failed to write play_list file"));
    }
    else
    {
        GtkTreeModel *model = gtk_tree_view_get_model(audio_project->tree);
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        while (valid)
        {
            gchar *playlist_item = NULL;
            gtk_tree_model_get (model, &iter,  AUDIO_COL_FILE, &playlist_item, -1);
            fprintf(file, "%s\n", playlist_item);
            g_free (playlist_item);
            valid = gtk_tree_model_iter_next (model, &iter);
        }
        
        fclose(file);
        ret = TRUE;
    }
    
    return ret;
}    


static gboolean
audioproject_export_pls(AudioProject *audio_project, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    FILE *file;   
    if ((file = fopen(play_list, "w")) == 0)
    {
        g_warning("audioproject_export_pls - Failed to write play_list [%s]", play_list);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, 
            GTK_BUTTONS_NONE, _("Failed to write play_list file"));
    }
    else
    {
        fprintf(file, "[play_list]\n");
        GtkTreeModel *model = gtk_tree_view_get_model(audio_project->tree);
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        gint track = 1;
        while (valid)
        {
            gchar *playlist_item = NULL, *title = NULL;
            MediaInfo *info = NULL;
            gtk_tree_model_get (model, &iter, AUDIO_COL_FILE, &playlist_item, -1);
            gtk_tree_model_get (model, &iter, AUDIO_COL_TITLE, &title, -1);
            gtk_tree_model_get (model, &iter, AUDIO_COL_INFO, &info, -1);
            fprintf(file, "File%d=%s\n", track, playlist_item);
            fprintf(file, "Title%d=%s\n", track, title);
            fprintf(file, "Length%d=%ld\n", track, info->duration);
            g_free (playlist_item);
            g_free (title);
            /*g_free (info); we don't own the media info pointer */
            
            valid = gtk_tree_model_iter_next (model, &iter);
            track++;
        }

        fprintf(file, "NumberOfEntries=%d\n", track-1);
        fprintf(file, "Version=2");
        fclose(file);
        ret = TRUE;
    }
    
    return ret;
}    


gboolean 
audioproject_export_playlist(AudioProject *audio_project, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    gint result = GTK_RESPONSE_YES;
    if(g_file_test(play_list, G_FILE_TEST_EXISTS))
    {
        result = gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                    GTK_BUTTONS_NONE, _("Playlist already exists.\n Do you want to replace it?"));
    }

    if(result == GTK_RESPONSE_YES)
    {
        if (strlen(play_list) < 4 || gbcommon_str_has_suffix(play_list, ".m3u"))
            ret = audioproject_export_m3u(audio_project, play_list);
        else if (strlen(play_list) < 4 || gbcommon_str_has_suffix(play_list, ".pls"))
            ret = audioproject_export_pls(audio_project, play_list);
    }   
    return ret;
}



static void
audioproject_clear(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
    AudioProject *audio_project = AUDIOPROJECT_WIDGET(project);
    
    gnomebaker_show_busy_cursor(TRUE);
    
    audio_project->compilation_seconds = 0.0;
    
    GtkTreeModel *file_model = gtk_tree_view_get_model(audio_project->tree);    
    gchar *buf = audioproject_format_progress_text(audio_project);
    gtk_progress_bar_set_text(project->progress_bar, buf);
    g_free(buf);
    gtk_progress_bar_set_fraction(project->progress_bar, 0.0);

    gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(audio_project)->button), FALSE);

    /* Now get each row and free the MediaInfo* we store in each */    
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(file_model, &iter))
    {
        do
        {
            MediaInfo *info = NULL;
            gtk_tree_model_get (file_model, &iter, AUDIO_COL_INFO, &info, -1);
            media_info_delete(info);
        } while (gtk_tree_model_iter_next(file_model, &iter));
    }    
    gtk_list_store_clear(GTK_LIST_STORE(file_model));
    gnomebaker_show_busy_cursor(FALSE); 
}


static void
audioproject_remove(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
    AudioProject *audio_project = AUDIOPROJECT_WIDGET(project);
    
    gnomebaker_show_busy_cursor(TRUE);
    
    GtkTreeModel *file_model = gtk_tree_view_get_model(audio_project->tree);    
    
    GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(audio_project->tree);
    gtk_tree_selection_selected_foreach(selection, audioproject_foreach_fileselection, &rr_list);        
    
    GList *node = rr_list;
    for (; node != NULL ; node = node->next)
    {
        if(node->data)
        {
            GtkTreePath *path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
            
            if (path)
            {
                GB_DECLARE_STRUCT(GtkTreeIter, iter);
                if (gtk_tree_model_get_iter(file_model, &iter, path))
                {
                    MediaInfo *info = NULL;
                    gtk_tree_model_get (file_model, &iter, AUDIO_COL_INFO, &info, -1);
                    audioproject_update_progress_bar(audio_project, FALSE, (gdouble)info->duration);  
                    gtk_list_store_remove(GTK_LIST_STORE(file_model), &iter);
                    media_info_delete(info);
                }           
                /* FIXME/CHECK: Do we need to free the path here? */
            }
        }
    }
    
    g_list_foreach(rr_list, (GFunc)gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);       
    
    gnomebaker_show_busy_cursor(FALSE);    
}


static void 
audioproject_on_remove_clicked(GtkWidget *menuitem, AudioProject *audio_project)
{
    GB_LOG_FUNC 
    audioproject_remove(PROJECT_WIDGET(audio_project));
}


static void 
audioproject_on_clear_clicked(GtkWidget *menuitem, AudioProject *audio_project)
{
    GB_LOG_FUNC
    audioproject_clear(PROJECT_WIDGET(audio_project));
}


static gboolean
audioproject_on_button_pressed(GtkWidget *widget, GdkEventButton *event, AudioProject *audio_project)
{
    GB_LOG_FUNC
    g_return_val_if_fail(widget != NULL, FALSE);

    /* look for a right click */    
    if(event->button == 3)
    {
        GtkWidget *menu = gtk_menu_new();
        GtkTreeView *view = (GtkTreeView*)widget;
        GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
        const gint count = gtk_tree_selection_count_selected_rows(selection);
        if(count == 1)
        {
            gbcommon_append_menu_item_stock(menu, _("_Open"), GTK_STOCK_OPEN, 
                (GCallback)audioproject_on_open, view);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        }
        
        gbcommon_append_menu_item_stock(menu, _("Move selected _up"), GTK_STOCK_GO_UP, 
            (GCallback)audioproject_on_move_up, audio_project);
        gbcommon_append_menu_item_stock(menu, _("Move selected _down"), GTK_STOCK_GO_DOWN,
            (GCallback)audioproject_on_move_down, audio_project);   
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());        
        gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE, 
            (GCallback)audioproject_on_remove_clicked, audio_project);  
        gbcommon_append_menu_item_stock(menu, _("_Clear"), GTK_STOCK_CLEAR,
            (GCallback)audioproject_on_clear_clicked, audio_project);   
            
        gtk_widget_show_all(menu);
    
        /* Note: event can be NULL here when called. However,
         *  gdk_event_get_time() accepts a NULL argument */
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                       (event != NULL) ? event->button : 0,
                       gdk_event_get_time((GdkEvent*)event));
        return TRUE;
    }
    return FALSE;   
}


static void
audioproject_add_selection(Project *project, GtkSelectionData *selection)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
    g_return_if_fail(selection != NULL);
    g_return_if_fail(selection->data != NULL);
    
    AudioProject *audio_project = AUDIOPROJECT_WIDGET(project);
    GtkTreeView *view = audio_project->tree;
    g_return_if_fail(view != NULL);                         
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    g_return_if_fail(model != NULL);
    
    gnomebaker_show_busy_cursor(TRUE);

    gboolean cont = TRUE;
    const gchar *file = strtok((gchar*)selection->data, "\n");
    while((file != NULL) && cont)
    {
        /* Get the file name that's been dropped and if there's a 
           file url at the start then strip it off */   
        gchar *file_name = gbcommon_get_local_path(file);        
        
        /* if the file is really a directory then we open it
        and attempt to add any audio files in the directory */
        if(g_file_test(file_name, G_FILE_TEST_IS_DIR))
        {
            GDir *dir = g_dir_open(file_name, 0, NULL);
            const gchar *name = g_dir_read_name(dir);
            while((name != NULL) && cont)
            {
                gchar *full_name = g_build_filename(file_name, name, NULL);
                if(!g_file_test(full_name, G_FILE_TEST_IS_DIR))
                    cont = audioproject_add_file(audio_project, file_name);
                g_free(full_name);
                name = g_dir_read_name(dir);                    
            }
            g_dir_close(dir);
        }
        else 
        {
            /* Now try and find any play_list types and add them */
            gchar *mime = gbcommon_get_mime_type(file_name);
            if(audioproject_is_supported_playlist(mime))
                cont = audioproject_import_supported_playlist(audio_project, mime, file_name);
            else 
                cont = audioproject_add_file(audio_project, file_name);
            g_free(mime);
        }
        g_free(file_name);       
        file = strtok(NULL, "\n");
    }
    
    gnomebaker_show_busy_cursor(FALSE);
}


static void
audioproject_on_drag_data_received(
    GtkWidget  *widget,
    GdkDragContext  *context,
    gint x,
    gint y,
    GtkSelectionData  *selection_data,
    guint info,
    guint time,
    AudioProject *audio_project)
{
    GB_LOG_FUNC
    audioproject_add_selection(PROJECT_WIDGET(audio_project), selection_data);
}


static void
audioproject_import_session(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
}


static void
audioproject_open(Project *project, const gchar *file_name)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
    g_return_if_fail(file_name != NULL);
}


static void
audioproject_save(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
}


static void
audioproject_close(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(AUDIOPROJECT_IS_WIDGET(project));
}


static void
audioproject_class_init(AudioProjectClass *klass)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET_CLASS(klass));    
    
    ProjectClass *project_class = PROJECT_WIDGET_CLASS(klass);
    project_class->clear = audioproject_clear;
    project_class->remove = audioproject_remove;
    project_class->add_selection = audioproject_add_selection;
    project_class->import_session = audioproject_import_session;
    project_class->open = audioproject_open;
    project_class->save = audioproject_save;
    project_class->close = audioproject_close;
    project_class->move_selected_up = audioproject_move_selected_up;
    project_class->move_selected_down = audioproject_move_selected_down;
}


static void
audioproject_init(AudioProject *audio_project)
{
    GB_LOG_FUNC
    g_return_if_fail(audio_project != NULL);
    
    audio_project->compilation_seconds = 0.0;
    
    GtkWidget *scrolledwindow18 = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow18);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow18), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(audio_project), scrolledwindow18, TRUE, TRUE, 0);
    gtk_box_reorder_child(GTK_BOX(audio_project), scrolledwindow18, 0);

    audio_project->tree = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_widget_show(GTK_WIDGET(audio_project->tree));
    gtk_container_add(GTK_CONTAINER(scrolledwindow18), GTK_WIDGET(audio_project->tree));
  
    /* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(AUDIO_NUM_COLS, 
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, /*G_TYPE_ULONG,*/
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model(audio_project->tree, GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* One column which has an icon renderer and text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Track"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", AUDIO_COL_ICON, NULL);
    
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_FILE, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);

    /* Second column to display the duration*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Duration"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_DURATION, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);
    
    /* Third column to display the size 
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Size");
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_SIZE, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);*/
        
    /* column to display the artist */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Artist"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_ARTIST, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);
    
    /* column to display the album */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Album"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_ALBUM, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);
    
    /* column to display the title */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Title"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIO_COL_TITLE, NULL);
    gtk_tree_view_append_column(audio_project->tree, col);
    
    /* hidden column to store the MediaInfo pointer */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Info"));
    gtk_tree_view_append_column(audio_project->tree, col);
    gtk_tree_view_column_set_visible(col, FALSE);
    
    /* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(audio_project->tree), GTK_SELECTION_MULTIPLE);

    /* Enable the file list as a drag destination */    
    gtk_drag_dest_set(GTK_WIDGET(audio_project->tree), GTK_DEST_DEFAULT_ALL,
        target_entries, TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    /* Connect the function to handle the drag data */
    g_signal_connect(audio_project->tree, "drag_data_received",
            G_CALLBACK(audioproject_on_drag_data_received), audio_project);
    
    /* connect the signal to handle right click */
    g_signal_connect (G_OBJECT(audio_project->tree), "button-press-event",
            G_CALLBACK(audioproject_on_button_pressed), audio_project);
        
    g_signal_connect(G_OBJECT(audio_project->tree), "row-activated", 
            G_CALLBACK(audioproject_on_list_dbl_click), audio_project);
            
    g_signal_connect(G_OBJECT(PROJECT_WIDGET(audio_project)->button), "clicked", 
            G_CALLBACK(audioproject_on_create_audiocd), audio_project);

    g_signal_connect(G_OBJECT(PROJECT_WIDGET(audio_project)->menu), "changed", 
            G_CALLBACK(audioproject_on_audioproject_size_changed), audio_project);                
            
    gbcommon_populate_disk_size_option_menu(PROJECT_WIDGET(audio_project)->menu, audio_disk_sizes, 
            (sizeof(audio_disk_sizes)/sizeof(DiskSize)), preferences_get_int(GB_AUDIO_DISK_SIZE));      
            
    audioproject_get_audioproject_size(audio_project);
        
    project_set_title(PROJECT_WIDGET(audio_project), _("<b>Audio project</b>"));
}


GtkWidget*
audioproject_new()
{
    GB_LOG_FUNC
    return GTK_WIDGET(g_object_new(AUDIOPROJECT_TYPE_WIDGET, NULL));
}


