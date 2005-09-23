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
exec_spawn_process(ExecCmd* e, GSpawnChildSetupFunc child_setup, gboolean read)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	exec_print_cmd(e);
	gint stdout = 0, stderr = 0;		
    GError* err = NULL;
	g_mutex_lock(e->mutex);
	gboolean ok = g_spawn_async_with_pipes(NULL, e->argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD , 
        child_setup, e, &e->pid, NULL, &stdout, &stderr, &err);	
    g_mutex_unlock(e->mutex);
	if(ok)
	{
		GB_TRACE("exec_spawn_process - spawed process with pid [%d]", e->pid);
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
			err->code, err->message);			
        exec_cmd_set_state(e, FAILED, FALSE);
        g_error_free(err);
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

	GList* cmd = (GList*)data;
    for(; cmd != NULL; cmd = cmd->next)
	{
        ExecCmd* e = (ExecCmd*)cmd->data;
        if(e->libProc != NULL)
            e->libProc(e, child_child_pipe);
		else 
            exec_spawn_process(e, exec_stdout_setup_func, FALSE);
        
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
    GList* cmd = ex->cmds;
    for(; cmd != NULL; cmd = cmd->next)
        exec_cmd_set_state((ExecCmd*)cmd->data, FAILED, TRUE);
}


static gpointer
exec_thread(gpointer data)
{
    GB_LOG_FUNC
    g_return_val_if_fail(data != NULL, NULL);   

    Exec* ex = (Exec*)data;

    if(ex->startProc) ex->startProc(ex, NULL);
    ExecState state = RUNNABLE;
    GList* piped = NULL;    
    GList* cmd = ex->cmds;
    for(; cmd != NULL && ((state != CANCELLED) && (state != FAILED)); cmd = cmd->next)
    {
        ExecCmd* e = (ExecCmd*)cmd->data;
        if(e->piped) 
        {
            piped = g_list_append(piped, e);
            piped = g_list_first(piped);
            continue;
        }
        if(e->preProc) e->preProc(e, NULL);                 
        
        state = exec_cmd_get_state(e);
        if(state == SKIP) continue;
        else if(state == CANCELLED) break;    
        
        if(e->libProc != NULL)
        {
            e->libProc(e, NULL);
        }
        else if(piped != NULL)
        {
            pipe(child_child_pipe); 
            g_thread_create(exec_run_remainder, (gpointer)piped, TRUE, NULL);
            exec_spawn_process(e, exec_stdin_setup_func, TRUE);
            close(child_child_pipe[0]);
            close(child_child_pipe[1]); 
        }
        else                       
        {
            exec_spawn_process(e, exec_working_dir_setup_func, TRUE);
        }
            
        state = exec_cmd_get_state(e);
        if(((state == CANCELLED) || (state == FAILED)) && (piped != NULL))
            exec_stop_remainder(ex);
         
        if(e->postProc) e->postProc(e, NULL);
        
        g_list_free(piped);
        piped = NULL;
    }
    if(ex->endProc) ex->endProc(ex, NULL);
    GB_TRACE("exec_thread - exiting");
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


ExecCmd* 
exec_cmd_new(Exec* exec)
{
    GB_LOG_FUNC 
    g_return_val_if_fail(exec != NULL, NULL);
    
    ExecCmd* e = g_new0(ExecCmd, 1);
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
    exec->cmds = g_list_append(exec->cmds, e);
    exec->cmds = g_list_first(exec->cmds);
    return e;
}


void
exec_delete(Exec * exec)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != exec);
    g_free(exec->processtitle);
    g_free(exec->processdescription);
    GList* cmd = exec->cmds;
    for(; cmd != NULL; cmd = cmd->next)
        exec_cmd_delete((ExecCmd*)cmd->data);
    g_list_free(exec->cmds);
    if(exec->err != NULL)
        g_error_free(exec->err);
    g_free(exec);
}


Exec*
exec_new(const gchar* processtitle, const gchar* processdescription)
{
	GB_LOG_FUNC
	
	Exec* exec = g_new0(Exec, 1);
	g_return_val_if_fail(exec != NULL, NULL);        
    exec->processtitle = g_strdup(processtitle);
    exec->processdescription = g_strdup(processdescription);
	return exec;
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


void 
exec_cmd_update_arg(ExecCmd* e, const gchar* argstart, const gchar* value)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(argstart != NULL);
    g_return_if_fail(value != NULL);
    
    gint size = g_strv_length(e->argv);
    gint i = 0;
    for(; i < size; ++i)
    {
        if(strstr(e->argv[i], argstart) != NULL)
        {
            g_free(e->argv[i]);
            e->argv[i] = g_strconcat(argstart, value, NULL);
            GB_TRACE("exec_cmd_update_arg - set to [%s]", e->argv[i]);
            break;
        }
    }
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
    
    e->thread = g_thread_create(exec_thread, (gpointer) e, TRUE, &e->err);
    if(e->err != NULL)
    {
        g_critical("exec_go - failed to create thread [%d] [%s]",
               e->err->code, e->err->message);                              
    }
    return e->thread;
}


void
exec_stop(Exec* e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    
    GList* cmd = e->cmds;
    for(; cmd != NULL; cmd = cmd->next)
        exec_cmd_set_state((ExecCmd*)cmd->data, CANCELLED, TRUE);        
    g_thread_join(e->thread);
    
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


gint 
exec_count_operations(const Exec* e)
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, 0);   
    
    gint count = 0;
    GList* cmd = e->cmds;
    for(; cmd != NULL; cmd = cmd->next)
    {
        if(!((ExecCmd*)cmd->data)->piped)
            ++count;
    }
    GB_TRACE("exec_count_operations - there are [%d] operations", count);
    return count;
}


ExecState 
exec_get_outcome(const Exec* e)
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, FAILED);   
    
    ExecState outcome = COMPLETE;
    GList* cmd = e->cmds;
    for(; cmd != NULL; cmd = cmd->next)
    {
        const ExecState state = exec_cmd_get_state((ExecCmd*)cmd->data);
        if(state == CANCELLED)
        {           
            outcome = CANCELLED;
            break;
        }
        else if(state == FAILED)
        {            
            outcome = FAILED;
            break;
        }
    }
    return outcome;
}

