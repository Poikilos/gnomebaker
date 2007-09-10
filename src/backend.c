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
 * File: backend.c
 * Copyright: Andreas Kühntopf <andreas@kuehntopf.org>
 */

#include "backend.h"

 
gboolean
backend_is_backend_supported(enum backend b)
{
	switch (b)
	{
		case BACKEND_CDRECORD:
			return backend_does_prog_exist("cdrecord");
		case BACKEND_WODIM:
			return backend_does_prog_exist("wodim");
			
	}
}

gboolean
backend_does_prog_exist(gchar* program)
{
	gchar* prog = g_find_program_in_path(program);		
	if(prog != NULL
		&& !g_file_test(prog, G_FILE_TEST_IS_SYMLINK) 
		&& g_file_test(prog, G_FILE_TEST_IS_EXECUTABLE))
		return TRUE;
	else
		return FALSE;
}

