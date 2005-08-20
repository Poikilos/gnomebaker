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
/*typedef gboolean (*ExecLibFunc) (void *, gint*);*/

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
}ExecCmd;

typedef struct
{
	gint cmdCount;
	ExecCmd* cmds;
	ExecFunc startProc;
	ExecFunc endProc;
	GError *err;
} Exec;


Exec* exec_new(const gint cmds);
void exec_delete(Exec* self);
ExecCmd* exec_add_cmd(Exec* self);
gint exec_go (Exec * const e, gboolean onthefly);
void exec_cmd_add_arg (ExecCmd * const e, const gchar * const format, const gchar* const value);
void exec_cancel (const Exec * const e);
GString* exec_run_cmd(const gchar* cmd);
void exec_cmd_lock(ExecCmd* e);
void exec_cmd_unlock(ExecCmd* e);

#endif	/*_EXEC_H_*/
