/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * File: exec.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Feb 26 00:33:10 2003
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <glib.h>
#include "exec.h"
#include <glib/gprintf.h>
#include "gbcommon.h"


static gint child_child_pipe[2];


static void
exec_print_cmd(const ExecCmd* e)
{
	if(showtrace)
	{
		gint i = 0;
		for(; i < e->argc; i++)
			g_print("%s ", e->argv[i]);
		g_print("\n");		
	}
}


static gboolean 
exec_channel_callback(GIOChannel *channel, GIOCondition condition, gpointer data)
{   
    GB_LOG_FUNC
    ExecCmd* cmd = (ExecCmd*)data;
    gboolean cont = TRUE;
    
    if(condition & G_IO_IN || condition & G_IO_PRI) /* there's data to be read */
    {
        static const gint BUFF_SIZE = 1024;
        gchar buffer[BUFF_SIZE];
        gbcommon_memset(buffer, BUFF_SIZE * sizeof(gchar));
        guint bytes = 0;
        const GIOStatus status = g_io_channel_read_chars(channel, buffer, (BUFF_SIZE - 1) * sizeof(gchar), &bytes, NULL);  
        if (status == G_IO_STATUS_ERROR || status == G_IO_STATUS_AGAIN) /* need to check what to do for again */
        {
            GB_TRACE("exec_channel_callback - read error [%d]", status);
            cont = FALSE;
        }        
        else if(cmd->readProc) 
        {
            GError* error = NULL;        
            gchar* converted = g_convert(buffer, bytes, "UTF-8", "ISO-8859-1", NULL, NULL, &error);
            if(converted != NULL)
            {
                cmd->readProc(cmd, converted);
                g_free(converted);
            }
            else 
            {
                g_warning("exec_channel_callback - conversion error [%s]", error->message);
                g_error_free (error);
                cmd->readProc(cmd, buffer); 
            }
        }
    }
    
    if (cont == FALSE || condition & G_IO_HUP || condition & G_IO_ERR || condition & G_IO_NVAL) 
    {
        GB_TRACE("exec_channel_callback - condition [%d]", condition);
        g_mutex_lock(cmd->mutex);
        g_cond_broadcast(cmd->cond);
        g_mutex_unlock(cmd->mutex);
        cont = FALSE;
    }
    
    return cont;
}


static void
exec_spawn_process(Exec* ex, ExecCmd* e, GSpawnChildSetupFunc child_setup, gboolean read, GThreadFunc func)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);
	g_return_if_fail(e != NULL);
	
	exec_print_cmd(e);
	gint stdout = 0, stderr = 0;		
	g_mutex_lock(e->mutex);
	gboolean ok = g_spawn_async_with_pipes(NULL, e->argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD , 
        child_setup, e, &e->pid, NULL, &stdout, &stderr, &ex->err);	
    g_mutex_unlock(e->mutex);
	if(ok)
	{
		GB_TRACE("exec_spawn_process - spawed process with pid [%d]", e->pid);
		if(func != NULL)
			g_thread_create(func, (gpointer) ex, TRUE, &ex->err);
		GIOChannel* chanout = NULL;
		GIOChannel* chanerr = NULL;
		guint chanoutid = 0;
		guint chanerrid = 0;
		
		if(read)
		{
			chanout = g_io_channel_unix_new(stdout);			
			g_io_channel_set_encoding(chanout, NULL, NULL);
			g_io_channel_set_buffered(chanout, FALSE);
			g_io_channel_set_flags(chanout, G_IO_FLAG_NONBLOCK, NULL );
			chanoutid = g_io_add_watch(chanout, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL,
				exec_channel_callback, (gpointer)e);
		  
			chanerr = g_io_channel_unix_new(stderr);
			g_io_channel_set_encoding(chanerr, NULL, NULL);			
			g_io_channel_set_buffered(chanerr, FALSE);			
			g_io_channel_set_flags( chanerr, G_IO_FLAG_NONBLOCK, NULL );
			chanerrid = g_io_add_watch(chanerr, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL ,
				exec_channel_callback, (gpointer)e);
		}
		
		/* wait until we're told that the process is complete or cancelled */	
		int retcode = 0;
		g_mutex_lock(e->mutex);
		while((waitpid(e->pid, &retcode, WNOHANG)) != -1)
		{
            GB_DECLARE_STRUCT(GTimeVal, time);
            g_get_current_time(&time);
            g_time_val_add(&time, 1 * G_USEC_PER_SEC);
            if(g_cond_timed_wait(e->cond, e->mutex, &time))
                break;
		}
		/* If the process was cancelled then we kill of the child */
		if(e->state == CANCELLED)
			kill(e->pid, SIGKILL);			
		g_mutex_unlock(e->mutex);
		
		/* Reap the child so we don't get a zombie */
        waitpid(e->pid, &retcode, 0);
        
        g_mutex_lock(e->mutex);
        e->exitCode = retcode;
        g_mutex_unlock(e->mutex);
	
		if(read)
		{
			g_source_remove(chanoutid);
			g_source_remove(chanerrid);	
			g_io_channel_shutdown(chanout, FALSE, NULL);
			g_io_channel_unref(chanout);  
			g_io_channel_shutdown(chanerr, FALSE, NULL);
			g_io_channel_unref(chanerr);			
		}
		g_spawn_close_pid(e->pid);
		close(stdout);
		close(stderr);	
		
        exec_cmd_set_state(e, (e->exitCode == 0) ? COMPLETE : FAILED, FALSE);
		GB_TRACE("exec_spawn_process - child exitcode [%d]", e->exitCode);
	}
	else
	{
		g_critical("exec_spawn_process - failed to spawn process [%d] [%s]",
			ex->err->code, ex->err->message);			
        exec_cmd_set_state(e, FAILED, FALSE);
	}
}


static void
exec_working_dir_setup_func(gpointer data)
{
    GB_LOG_FUNC
    g_return_if_fail(data != NULL);
    ExecCmd* ex = (ExecCmd*)data;
    if(ex->workingdir != NULL)
        g_return_if_fail(chdir(ex->workingdir) == 0);
}


static void
exec_stdout_setup_func(gpointer data)
{
	GB_LOG_FUNC
    g_return_if_fail(data != NULL);
    exec_working_dir_setup_func(data);
	dup2(child_child_pipe[1], 1);
	close(child_child_pipe[0]);
}


static void
exec_stdin_setup_func(gpointer data)
{
	GB_LOG_FUNC	
    g_return_if_fail(data != NULL);
    exec_working_dir_setup_func(data);
	dup2(child_child_pipe[0], 0);
	close(child_child_pipe[1]);
}


static gpointer
exec_run_remainder(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;
	gint j = 0;
	for(; j < ex->cmdCount - 1; j++)
	{
        ExecCmd* e = &ex->cmds[j];
        if(e->libProc != NULL)
            e->libProc(e, child_child_pipe);
		else 
            exec_spawn_process(ex, e, exec_stdout_setup_func, FALSE, NULL);
        
        ExecState state = exec_cmd_get_state(e);
        if((state == CANCELLED) || (state == FAILED))
            break;
	}	
	close(child_child_pipe[1]);			
	GB_TRACE("exec_run_remainder - thread exiting");
	return NULL;
}


static void
exec_stop_remainder(Exec* ex)
{
    GB_LOG_FUNC
    gint j = 0;
    for(; j < ex->cmdCount - 1; j++)
        exec_cmd_set_state(&ex->cmds[j], FAILED, TRUE);
}


static gpointer
exec_thread_gspawn_otf(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;
	if(ex->startProc) ex->startProc(ex, NULL);	
	ExecCmd* e = &ex->cmds[ex->cmdCount - 1];
	if(e->preProc) e->preProc(e, NULL);				
        
    const ExecState state = exec_cmd_get_state(e);
    // TODO the state here needs sorting
	if((state != SKIP) && (state != CANCELLED))
	{
		pipe(child_child_pipe);	
		exec_spawn_process(ex, e, exec_stdin_setup_func, TRUE, exec_run_remainder);
        ExecState state = exec_cmd_get_state(e);
        if((state == CANCELLED) || (state == FAILED))
            exec_stop_remainder(ex);
	    if(e->postProc) e->postProc(e, NULL);	
		close(child_child_pipe[0]);	
		close(child_child_pipe[1]);
	}	
	if(ex->endProc) ex->endProc(ex, NULL);	
	GB_TRACE("exec_thread_gspawn_otf - exiting");
	return NULL;
}


static gpointer
exec_thread_gspawn(gpointer data)
{
    GB_LOG_FUNC
    g_return_val_if_fail(data != NULL, NULL);   

    Exec* ex = (Exec*)data;

    if(ex->startProc) ex->startProc(ex, NULL);

    gint j = 0;
    gboolean cont = TRUE;
    for(; j < ex->cmdCount && cont; j++)
    {
        ExecCmd* e = &ex->cmds[j];              
        if(e->preProc) e->preProc(e, NULL);                 
        
        ExecState state = exec_cmd_get_state(e);
        if(state == SKIP) continue;
        else if(state == CANCELLED) break;    
        
        if(e->libProc != NULL)
            e->libProc(e, NULL);
        else                       
            exec_spawn_process(ex, e, exec_working_dir_setup_func, TRUE, NULL);
         
        state = exec_cmd_get_state(e);
        cont = (state != CANCELLED) && (state != FAILED);
         
        if(e->postProc) e->postProc(e, NULL);
    }
    if(ex->endProc) ex->endProc(ex, NULL);
    GB_TRACE("exec_thread_gspawn - exiting");
    return NULL;
}


static void
exec_cmd_delete(ExecCmd * e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);

    g_strfreev(e->argv);
    g_cond_free(e->cond);
    g_mutex_free(e->mutex);    
    g_free(e->workingdir);
}


static ExecCmd* 
exec_cmd_new(Exec* self)
{
    GB_LOG_FUNC 
    g_return_val_if_fail(self != NULL, NULL);
    
    self->cmds = g_realloc(self->cmds, (++self->cmdCount) * sizeof(ExecCmd));
    ExecCmd* e = &(self->cmds[self->cmdCount - 1]);
    e->argc = 1;
    e->argv = g_malloc(sizeof(gchar*));
    e->argv[0] = NULL;
    e->pid = 0;
    e->exitCode = 0;
    e->state = RUNNABLE;
    e->libProc = NULL;
    e->preProc = NULL;
    e->readProc = NULL;
    e->postProc = NULL;
    e->mutex = g_mutex_new();
    e->cond = g_cond_new();
    e->workingdir = NULL;
    return e;
}


void
exec_delete(Exec * self)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != self);
    gint j = 0;
    for(; j < self->cmdCount; j++)
        exec_cmd_delete(&self->cmds[j]);
    g_free(self->cmds);
    if(self->err != NULL)
        g_error_free(self->err);
    g_free(self);
}


Exec*
exec_new(const gint cmds)
{
	GB_LOG_FUNC
	
	Exec* self = g_new0(Exec, 1);
	g_return_val_if_fail(self != NULL, NULL);    
    gint i = 0;
    for(; i < cmds; i++)
        exec_cmd_new(self);	
    self->onthefly = FALSE;
	return self;
}


void
exec_cmd_add_arg(ExecCmd* e, const gchar* format, const gchar* value)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(format != NULL);
    g_return_if_fail(value != NULL);

    e->argv = g_realloc(e->argv, (++e->argc) * sizeof(gchar*)); 
    e->argv[e->argc - 2] = g_strdup_printf(format, value);
    e->argv[e->argc - 1] = NULL;
}


gboolean 
exec_cmd_wait_for_signal(ExecCmd* e, guint timeinseconds)
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, FALSE);
    
    GB_DECLARE_STRUCT(GTimeVal, time);
    g_get_current_time(&time);
    g_time_val_add(&time, timeinseconds * G_USEC_PER_SEC);
    g_mutex_lock(e->mutex);        
    gboolean signalled = g_cond_timed_wait(e->cond, e->mutex, &time);
    g_mutex_unlock(e->mutex);    
    return signalled;
}


ExecState 
exec_cmd_get_state(ExecCmd* e) 
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, FAILED);
    
    g_mutex_lock(e->mutex);
    ExecState ret = e->state;
    g_mutex_unlock(e->mutex);
    return ret;
}


ExecState 
exec_cmd_set_state(ExecCmd* e, ExecState state, gboolean signal) 
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, FAILED);
    
    g_mutex_lock(e->mutex);
    if(e->state != CANCELLED)
        e->state = state;
    ExecState ret = e->state;
    if(signal)
        g_cond_broadcast(e->cond);
    g_mutex_unlock(e->mutex);
    return ret;
}


GThread*
exec_go(Exec* e)
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, NULL);
    
    GThread* thread = g_thread_create(e->onthefly ? exec_thread_gspawn_otf: exec_thread_gspawn, (gpointer) e, TRUE, &e->err);
    if(e->err != NULL)
    {
        g_critical("exec_go - failed to create thread [%d] [%s]",
               e->err->code, e->err->message);                              
    }
    return thread;
}


void
exec_stop(Exec* e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    
    gint j = 0;
    for(; j < e->cmdCount; j++)
        exec_cmd_set_state(&e->cmds[j], CANCELLED, TRUE);
    
    GB_TRACE("exec_stop - complete");
}


GString* 
exec_run_cmd(const gchar* cmd)
{
    GB_LOG_FUNC
    GB_TRACE("exec_run_cmd - %s", cmd);
    g_return_val_if_fail(cmd != NULL, NULL);
    
    GString* ret = NULL;    
    gchar* stdout = NULL;
    gchar* stderr = NULL;
    gint status = 0;
    GError* error = NULL;
    
    if(g_spawn_command_line_sync(cmd, &stdout, &stderr, &status, &error))
    {
        ret = g_string_new(stdout); 
        g_string_append(ret, stderr);
        
        g_free(stdout);
        g_free(stderr);     
        /*GB_TRACE(ret->str);*/
    }
    else if(error != NULL)
    {       
        g_critical("error [%s] spawning command [%s]", error->message, cmd);        
        g_error_free(error);
    }
    else
    {
        g_critical("Unknown error spawning command [%s]", cmd);     
    }
    
    return ret;
}
