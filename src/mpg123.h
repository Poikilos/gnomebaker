/*
 * File: mpg123.h
 * Created by: User <Email>
 * Created on: Fri Oct 22 22:19:29 2004
 */

#ifndef _LAME_H_
#define _LAME_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>
#include "exec.h"

void mpg123_add_mp3_args(ExecCmd* cmd, gchar* file, gchar** convertedfile);

#endif	/*_LAME_H_*/
