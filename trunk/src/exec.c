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
    g_return_if_fail(e != NULL);
	if(showtrace)
	{
		gint i = 0;
		for(; i < e->args->len; i++)
			g_print("%s ", (gchar*)g_ptr_array_index(e->args, i));
		g_print("\n");		
	}
}


static void 
exec_set_outcome(Exec* e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    
    e->outcome = COMPLETED;    
    GList* cmd = e->cmds;
    for(; cmd != NULL; cmd = cmd->next)
    {
        const ExecState state = exec_cmd_get_state((ExecCmd*)cmd->data);
        if(state == CANCELLED)
        {           
            e->outcome = CANCELLED;
            break;
        }
        else if(state == FAILED)
        {            
            e->outcome = FAILED;
            break;
        }
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
        memset(buffer, 0x0, BUFF_SIZE * sizeof(gchar));
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
        /* We assume a failure here (even on G_IO_HUP) as exec_spawn_process will 
         check the return code of the child to determine if it actually worked
         and set the correct state accordingly.*/
        exec_cmd_set_state(cmd, FAILED);
        GB_TRACE("exec_channel_callback - condition [%d]", condition);        
        cont = FALSE;
    }
    
    return cont;
}


static void
exec_spawn_process(ExecCmd* e, GSpawnChildSetupFunc child_setup)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
    /* Make sure that the args are null terminated */
    g_ptr_array_add(e->args, NULL);
	exec_print_cmd(e);
	gint stdout = 0, stderr = 0;		
    GError* err = NULL;
	if(g_spawn_async_with_pipes(NULL, (gchar**)e->args->pdata, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD , 
        child_setup, e, &e->pid, NULL, &stdout, &stderr, &err))
	{
		GB_TRACE("exec_spawn_process - spawed process with pid [%d]", e->pid);
		GIOChannel *chanout = NULL, *chanerr = NULL;
		guint chanoutid = 0, chanerrid = 0;
		
		if(!e->piped)
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

            while(exec_cmd_get_state(e) == RUNNING)
                gtk_main_iteration();                               
                
            g_source_remove(chanoutid);
            g_source_remove(chanerrid); 
            g_io_channel_shutdown(chanout, FALSE, NULL);
            g_io_channel_unref(chanout);  
            g_io_channel_shutdown(chanerr, FALSE, NULL);
            g_io_channel_unref(chanerr);                
        }
        else
        {
            while((waitpid(e->pid, &e->exitCode, WNOHANG) != -1) && (exec_cmd_get_state(e) == RUNNING))
                g_usleep(500000);
        }
        
        /* If the process was cancelled then we kill off the child */
        if(exec_cmd_get_state(e) == CANCELLED)
            kill(e->pid, SIGKILL);
        
        /* Reap the child so we don't get a zombie */
        waitpid(e->pid, &e->exitCode, 0);
		g_spawn_close_pid(e->pid);
		close(stdout);
		close(stderr);	
		
        exec_cmd_set_state(e, (e->exitCode == 0) ? COMPLETED : FAILED);
		GB_TRACE("exec_spawn_process - child [%d] exitcode [%d]", e->pid, e->exitCode);
	}
	else
	{
		g_critical("exec_spawn_process - failed to spawn process [%d] [%s]",
			err->code, err->message);			
        exec_cmd_set_state(e, FAILED);
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
            exec_spawn_process(e, exec_stdout_setup_func);
        
        ExecState state = exec_cmd_get_state(e);
        if((state == CANCELLED) || (state == FAILED))
            break;
	}	
	close(child_child_pipe[1]);			
	GB_TRACE("exec_run_remainder - thread exiting");
	return NULL;
}


static void
exec_cmd_delete(ExecCmd * e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_mutex_free(e->statemutex);
    g_ptr_array_free(e->args, TRUE);
    g_free(e->workingdir);
}


ExecCmd* 
exec_cmd_new(Exec* exec)
{
    GB_LOG_FUNC 
    g_return_val_if_fail(exec != NULL, NULL);
    
    ExecCmd* e = g_new0(ExecCmd, 1);
    e->args = g_ptr_array_new();
    e->state = RUNNING;
    e->statemutex = g_mutex_new();
    exec->cmds = g_list_append(exec->cmds, e);
    exec->cmds = g_list_first(exec->cmds);
    return e;
}


void
exec_delete(Exec * exec)
{
    GB_LOG_FUNC
    g_return_if_fail(exec != NULL);
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
    g_return_val_if_fail(processtitle != NULL, NULL);
    g_return_val_if_fail(processdescription != NULL, NULL);
    
	Exec* exec = g_new0(Exec, 1);
	g_return_val_if_fail(exec != NULL, NULL);        
    exec->processtitle = g_strdup(processtitle);
    exec->processdescription = g_strdup(processdescription);
    exec->outcome = FAILED;
	return exec;
}


void
exec_cmd_add_arg(ExecCmd* e, const gchar* format, ...)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(format != NULL);

    va_list va; 
    va_start(va, format);
    gchar* arg = NULL;
    g_vasprintf(&arg, format, va);
    va_end(va);    
    g_ptr_array_add(e->args, arg);
}


void 
exec_cmd_update_arg(ExecCmd* e, const gchar* argstart, const gchar* format, ...)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(argstart != NULL);
    g_return_if_fail(format != NULL);

    gint i = 0;    
    for(; i < e->args->len; ++i)
    {
        gchar* arg = g_ptr_array_index(e->args, i);
        if(strstr(arg, argstart) != NULL)
        {
            g_free(arg);
            va_list va; 
            gchar* newarg = NULL;
            va_start(va, format);
            g_vasprintf(&newarg, format, va);
            va_end(va);    
            e->args->pdata[i] = newarg;
            GB_TRACE("exec_cmd_update_arg - set to [%s]", (gchar*)g_ptr_array_index(e->args, i));
            break;
        }
    }
}


ExecState 
exec_cmd_get_state(ExecCmd* e) 
{
    /*GB_LOG_FUNC*/
    g_return_val_if_fail(e != NULL, FAILED);
    g_mutex_lock(e->statemutex);
    ExecState ret = e->state;
    g_mutex_unlock(e->statemutex);
    return ret;
}


ExecState 
exec_cmd_set_state(ExecCmd* e, ExecState state)
{
    GB_LOG_FUNC
    g_return_val_if_fail(e != NULL, FAILED);
    g_mutex_lock(e->statemutex);
    if(e->state != CANCELLED)
        e->state = state;
    ExecState ret = e->state;
    g_mutex_unlock(e->statemutex);
    return ret;
}


void
exec_run(Exec* ex)
{    
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);   

    ExecState state = RUNNING;
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
        if(state == SKIPPED) continue;
        else if(state == CANCELLED) break;    
        
        GThread* thread = NULL;
        if(e->libProc != NULL) 
            e->libProc(e, NULL);
        else if(piped != NULL)
        {
            pipe(child_child_pipe); 
            thread = g_thread_create(exec_run_remainder, (gpointer)piped, TRUE, NULL);
            exec_spawn_process(e, exec_stdin_setup_func);
            close(child_child_pipe[0]);
            close(child_child_pipe[1]); 
        }
        else 
            exec_spawn_process(e, exec_working_dir_setup_func);
            
        state = exec_cmd_get_state(e);  
        if((/*(state == CANCELLED) || */(state == FAILED)) && (piped != NULL))
        {
            GList* cmd = ex->cmds;
            for(; cmd != NULL; cmd = cmd->next)
                exec_cmd_set_state((ExecCmd*)cmd->data, FAILED);
        } 
         
        if(e->postProc) e->postProc(e, NULL);
        if(thread != NULL) g_thread_join(thread);        
        
        g_list_free(piped);
        piped = NULL;
    }
    exec_set_outcome(ex);
}


void
exec_stop(Exec* e)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    
    GList* cmd = e->cmds;
    for(; cmd != NULL; cmd = cmd->next)
        exec_cmd_set_state((ExecCmd*)cmd->data, CANCELLED);        
    
    GB_TRACE("exec_stop - complete");
}


gint
exec_run_cmd(const gchar* cmd, gchar** output)
{
    GB_LOG_FUNC    
    g_return_val_if_fail(cmd != NULL, -1);
        
    gchar* stdout = NULL;
    gchar* stderr = NULL;
    gint exitcode = -1;
    GError* error = NULL;    
    if(g_spawn_command_line_sync(cmd, &stdout, &stderr, &exitcode, &error))
    {
        *output = g_strconcat(stdout, stderr, NULL);
        g_free(stdout);
        g_free(stderr);     
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
    GB_TRACE("exec_run_cmd - [%s] returned [%d]", cmd, exitcode);
    return exitcode;
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


