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
 * Created by: luke <luke@dhcp-45-369>
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


void exec_cmd_init(ExecCmd * e);
void exec_cmd_end(ExecCmd * e);
gboolean exec_init(Exec* self, const gint cmds);
void exec_end(Exec* self);


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


void
exec_delete(Exec * self)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	exec_end(self);
	g_free(self);
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
	e->argv[e->argc - 2] = g_malloc((strlen(format) + strlen(value) + 1) * sizeof(gchar));
	g_sprintf(e->argv[e->argc - 2], format, value);
	e->argv[e->argc - 1] = NULL;
}


gpointer
exec_thread(gpointer data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(data != NULL, NULL); 	

	Exec* ex = (Exec*)data;

	if(ex->startProc) ex->startProc(ex, NULL);

	gboolean cont = TRUE;
	gint j = 0;
	for(; j < ex->cmdCount && cont; j++)
	{
		ExecCmd* e = &ex->cmds[j];
		
		gint i = 0;
		for(; i < e->argc; i++)
			g_print("%s ", e->argv[i]);
		g_print("\n");

		if(e->preProc) e->preProc(e, NULL);		
			
		if(e->state == SKIP) continue;
		else if(e->state == CANCELLED) break;
				
		gint stdin_pipe[2];		
		pipe(stdin_pipe);

		pid_t pid = fork();
		if(pid)
		{
			/* Parent */
			e->pid = pid;
			g_message("exec_thread - parent created child with pid %d", e->pid);

			/* Connect to the downstream end of the stdin pipe and close
			 * the other end */
			dup2(stdin_pipe[0], 0);
			close(stdin_pipe[1]);

			static const gint BUFF_SIZE = 1024;
			gchar buffer[BUFF_SIZE];
			memset(buffer, 0, BUFF_SIZE);

			/* Give the process time to write to the pipe 
			fd_set set;
			GB_DECLARE_STRUCT(struct timeval, timeout);
			FD_ZERO (&set);
			FD_SET (stdin_pipe[0], &set);
			timeout.tv_sec = 4;
			timeout.tv_usec = 0;
			
			select(FD_SETSIZE, &set, NULL, NULL, &timeout);
			g_message( "Select returned - [%d]", selectresult);*/

			while(read(stdin_pipe[0], buffer, BUFF_SIZE) > 0)
			{				
				/*g_message( "Exec stdin pipe - %s", buffer);*/
				if(e->readProc)	e->readProc(e, buffer);
				memset(buffer, 0, BUFF_SIZE);
			}
		}
		else
		{
			/* Child */
			g_message("exec_thread - created child");

			/* Connect child stdout to the upstream end of the stdin pipe,
			 * Connect child std err to the upstream end of the stdin pipe
			 * and close the other end */
			dup2(stdin_pipe[1], 0);
			dup2(stdin_pipe[1], 1);
			dup2(stdin_pipe[1], 2);
			close(stdin_pipe[0]);

			execvp(e->argv[0], e->argv);

			/* If it didn't take over the Exec call failed.*/
			g_message("exec_thread - Exec failed");
		}

		wait(&e->exitCode);
		
		g_message("exec_thread - child exited with code [%d]\n", e->exitCode);

		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
	
		if(e->postProc) e->postProc(e, NULL);

		cont = (e->exitCode == 0);
		
		if(e->state != CANCELLED)
			e->state = (e->exitCode == 0) ? COMPLETE : FAILED;
	}

	if(ex->endProc) ex->endProc(ex, NULL);

	g_message( "exec_thread - exiting");

	return NULL;
}


gint
exec_go(Exec * const e)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, -1);
	
	gint ret = 0;

	g_thread_create(exec_thread,(gpointer) e, TRUE, &e->err);
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
			g_message( "exec_cancel - killing %d", e->cmds[j].pid);
			kill(e->cmds[j].pid, SIGKILL);
			e->cmds[j].state = CANCELLED;
		}
	}
	
	g_message( "exec_cancel - complete");
}


GString* 
exec_run_cmd(const gchar* cmd)
{
	GB_LOG_FUNC
	g_message( "exec_run_cmd - %s", cmd);
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
		/*g_message(ret->str);*/
	}
	else if(error != NULL)
	{		
		g_critical("error [%s] spawning command [%s]", error->message, cmd);		
		g_error_free(error);
	}
	else
	{
		g_critical("Uknown error spawning command [%s]", cmd);		
	}
	
	return ret;
}
