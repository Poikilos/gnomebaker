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
 * File: exec.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Feb 26 00:33:10 2003
 */

#ifndef _EXEC_H_
#define _EXEC_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <gnome.h>


typedef void (*ExecFunc) (void *, void*);

typedef enum
{
	RUNNABLE = 0,
	SKIP,
	CANCELLED,
	FAILED,
	COMPLETE
} ExecState;

typedef struct
{
	gint argc;
	gchar **argv;
	pid_t pid;
	gint exitCode;
	ExecState state;
    ExecFunc libProc;
	ExecFunc preProc;
	ExecFunc readProc;
	ExecFunc postProc;	
    GMutex* mutex;
    GCond* cond;
    gchar* workingdir;
    gboolean piped;
}ExecCmd;

typedef struct
{
    GThread* thread;
    gchar* processtitle;
    gchar* processdescription;
	GList* cmds;
	ExecFunc startProc;
	ExecFunc endProc;
	GError *err;
} Exec;


Exec* exec_new(const gchar* processtitle, const gchar* processdescription);
ExecCmd* exec_cmd_new(Exec* e);
void exec_delete(Exec* self);
GThread* exec_go(Exec* e);
void exec_cmd_add_arg(ExecCmd* e, const gchar* format, const gchar* value);
void exec_cmd_update_arg(ExecCmd* e, const gchar* arg, const gchar* value);
void exec_stop (Exec* e);
GString* exec_run_cmd(const gchar* cmd);
gboolean exec_cmd_wait_for_signal(ExecCmd* e, guint timeinseconds);
ExecState exec_cmd_get_state(ExecCmd* e);
ExecState exec_cmd_set_state(ExecCmd* e, ExecState state, gboolean signal);
gint exec_count_operations(const Exec* e);
ExecState exec_get_outcome(const Exec* e);

#endif	/*_EXEC_H_*/
