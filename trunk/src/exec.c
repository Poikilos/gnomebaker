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


static const gint BUFF_SIZE = 1024;

gboolean
exec_init(Exec * self, const gint cmds)
{
	GB_LOG_FUNC
	g_return_val_if_fail(self != NULL, FALSE);
	
	memset(self, 0x0, sizeof(Exec));
	
	gint i = 0;
	for(; i < cmds; i++)
		exec_add_cmd(self);

	return TRUE;
}


void
exec_cmd_init(ExecCmd * e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	e->argc = 1;
	e->argv = g_malloc(sizeof(gchar*));
	e->argv[0] = NULL;
	e->pid = 0;
	e->exitCode = 0;
	e->state = RUNNABLE;
	e->preProc = NULL;
	e->readProc = NULL;
	e->postProc = NULL;
}


ExecCmd* 
exec_add_cmd(Exec* self)
{
	GB_LOG_FUNC	
	g_return_val_if_fail(self != NULL, NULL);
	
	self->cmds = g_realloc(self->cmds, (++self->cmdCount) * sizeof(ExecCmd));
	ExecCmd* execcmd = &(self->cmds[self->cmdCount - 1]);
	exec_cmd_init(execcmd);
	return execcmd;
}


void
exec_cmd_end(ExecCmd * e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	gint i = 0;
	for(; i < e->argc; i++)
		g_free(e->argv[i]);

	g_free(e->argv);
}


void
exec_cmd_add_arg(ExecCmd * const e, const gchar * const format,
		  		 const gchar * const value)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(format != NULL);
	g_return_if_fail(value != NULL);

	e->argv = g_realloc(e->argv, (++e->argc) * sizeof(gchar*));	
	e->argv[e->argc - 2] = g_strdup_printf(format, value);
	e->argv[e->argc - 1] = NULL;
}


gint
exec_select(const gint fd, const guint timeout)
{
	fd_set set;
	GB_DECLARE_STRUCT(struct timeval, tval);
	FD_ZERO (&set);
	FD_SET (fd, &set);
	tval.tv_sec = timeout;
	tval.tv_usec = 0;
	return select(FD_SETSIZE, &set, NULL, NULL, &tval);	
}


void
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


void 
exec_read(ExecCmd* e, gint fd)
{
	GB_LOG_FUNC
	if(e->readProc)
	{
		gchar buffer[BUFF_SIZE];	
		gint res = 0;
		while((res = exec_select(fd, 2 /* seconds */)) != -1)
		{
			GB_TRACE( "exec_read - select returned [%d] errno [%d]", res, errno);
            if(res > 0)
            {
                memset(buffer, 0, BUFF_SIZE);
                if(read(fd, buffer, BUFF_SIZE) > 0)
                    e->readProc(e, buffer);		
            }
			
			if(waitpid(e->pid, &e->exitCode, WNOHANG) == -1) /* child no longer running */
				break;	

            /* this is a bit of a hack but it stops gb from hard looping and
               using up lots of cpu reading little fragments of data from the pipe.
               This way we wait until there's more data in the pipe and read less times. */
            g_usleep(500000);
		}	
		GB_TRACE( "exec_read - final select returned [%d] errno [%d]", res, errno);
	}
	waitpid(e->pid, &e->exitCode, 0);
	GB_TRACE("exec_read - child exited with code [%d]\n", e->exitCode);
}


gpointer
exec_thread(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;
	if(ex->startProc) ex->startProc(ex, NULL);

	gint j = 0;
	for(; j < ex->cmdCount; j++)
	{
		ExecCmd* e = &ex->cmds[j];
		exec_print_cmd(e);
		
		if(e->preProc) e->preProc(e, NULL);			
		if(e->state == SKIP) continue;
		else if(e->state == CANCELLED) break;
				
		gint stdin_pipe[2];		
		pipe(stdin_pipe);
		e->pid = fork();
		if(e->pid == 0)
		{
			/* Child */
			GB_TRACE("exec_thread - created child");

			/* Connect child stdout to the upstream end of the stdin pipe,
			 * Connect child std err to the upstream end of the stdin pipe
			 * and close the other end */
			dup2(stdin_pipe[1], STDIN_FILENO);
			dup2(stdin_pipe[1], STDOUT_FILENO);
			dup2(stdin_pipe[1], STDERR_FILENO);
			close(stdin_pipe[0]);

			execvp(e->argv[0], e->argv);

			/* If it didn't take over the Exec call failed.*/
			GB_TRACE("exec_thread - Exec failed");
		}

		/* Parent */
		GB_TRACE("exec_thread - parent created child with pid %d", e->pid);
		exec_read(e, stdin_pipe[0]);

		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
	
		if(e->postProc) e->postProc(e, NULL);		
		if(e->state != CANCELLED) e->state = (e->exitCode == 0) ? COMPLETE : FAILED;
		if(e->exitCode != 0) break;		
	}

	if(ex->endProc) ex->endProc(ex, NULL);
	GB_TRACE("exec_thread - exiting");
	return NULL;
}


gpointer
exec_run_remainder(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;
	gint j = 0;
	for(; j < ex->cmdCount - 1; j++)
	{
		ExecCmd* e = &ex->cmds[j];
		exec_print_cmd(e);
		e->pid = fork();
		if(e->pid == 0)
		{
			/* child */
			GB_TRACE("exec_run_remainder - child running");	
			dup2(ex->child_child_pipe[1], STDOUT_FILENO);
			/*dup2(ex->child_child_pipe[1], STDERR_FILENO); don't want stderr */
			close(ex->child_child_pipe[0]);	
			
			execvp(e->argv[0], e->argv);

			/* If it didn't take over the Exec call failed.*/
			GB_TRACE("exec_run_remainder - Exec failed");
		}
		/* parent */		
		GB_TRACE("exec_run_remainder - parent created child with pid %d", e->pid);
		waitpid(e->pid, &e->exitCode, 0);
		GB_TRACE("exec_run_remainder - child exited with code [%d]\n", e->exitCode);
		close(ex->child_child_pipe[1]);	
		if(e->exitCode != 0)
			break;		
	}	
	
	GB_TRACE("exec_run_remainder - thread exiting");
	return NULL;
}


gpointer
exec_thread_on_the_fly(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;
	if(ex->startProc) ex->startProc(ex, NULL);
	
	ExecCmd* e = &ex->cmds[ex->cmdCount - 1];
	exec_print_cmd(e);

	if(e->preProc) e->preProc(e, NULL);				
	if(e->state == SKIP) return NULL;
	else if(e->state == CANCELLED) return NULL;
			
	gint parent_child_pipe[2];
	pipe(parent_child_pipe);
	pipe(ex->child_child_pipe);	
	e->pid = fork();
	if(e->pid == 0)
	{
		/* Child */
		GB_TRACE("exec_thread_on_the_fly - root child running");
		/*const gint res = exec_select(ex->child_child_pipe[0], 4);
		GB_TRACE("exec_thread_on_the_fly - root child select returned [%d] errno [%d]", res, errno);*/
				
		dup2(ex->child_child_pipe[0], STDIN_FILENO);
		close(ex->child_child_pipe[1]);
		
		dup2(parent_child_pipe[1], STDOUT_FILENO);		
		dup2(parent_child_pipe[1], STDERR_FILENO);
		close(parent_child_pipe[0]);
					
		execvp(e->argv[0], e->argv);

		/* If it didn't take over the Exec call failed.*/
		GB_TRACE("exec_thread_on_the_fly - Exec failed");
	}
	
	/* Parent */
	
	/* Spool off the remaining exec instructions */
	g_thread_create(exec_run_remainder, data, TRUE, NULL);
			
	GB_TRACE("exec_thread_on_the_fly - parent created root child with pid %d", e->pid);		
	exec_read(e, parent_child_pipe[0]);
	
	close(parent_child_pipe[0]);
	close(parent_child_pipe[1]);
	close(ex->child_child_pipe[0]);
	close(ex->child_child_pipe[1]);	
	
	if(e->postProc) e->postProc(e, NULL);
	if(e->state != CANCELLED) e->state = (e->exitCode == 0) ? COMPLETE : FAILED;
	if(ex->endProc) ex->endProc(ex, NULL);

	GB_TRACE("exec_thread_on_the_fly - exiting");
	return NULL;
}


gint
exec_go(Exec * const e, gboolean onthefly)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, -1);
	
	gint ret = 0;
	g_thread_create(onthefly ? exec_thread_on_the_fly: exec_thread,
		(gpointer) e, TRUE, &e->err);
	if(e->err != NULL)
	{
		g_critical("exec_go - failed to create thread [%d] [%s]",
			   e->err->code, e->err->message);
		ret = e->err->code;
	}

	return ret;
}


void
exec_cancel(const Exec * const e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);

	gint j = 0;
	for(; j < e->cmdCount; j++)
	{
		if(e->cmds[j].pid > 0)
		{
			GB_TRACE("exec_cancel - killing %d", e->cmds[j].pid);
			kill(e->cmds[j].pid, SIGKILL);
			e->cmds[j].state = CANCELLED;
		}
	}
	
	GB_TRACE("exec_cancel - complete");
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


void
exec_end(Exec * self)
{
	GB_LOG_FUNC
	g_return_if_fail(self != NULL);
	
	gint j = 0;
	for(; j < self->cmdCount; j++)
		exec_cmd_end(&self->cmds[j]);

	g_free(self->cmds);
	if(self->err != NULL)
		g_error_free(self->err);
}


void
exec_delete(Exec * self)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	exec_end(self);
	g_free(self);
}


Exec*
exec_new(const gint cmds)
{
	GB_LOG_FUNC
	
	Exec* self = g_new(Exec, 1);
	g_return_val_if_fail(self != NULL, NULL);

	if(!exec_init(self, cmds))
	{
		g_free(self->cmds);
		g_free(self);
		self = NULL;
	}
	
	return self;
}
