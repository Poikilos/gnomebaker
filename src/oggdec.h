/*
 * File: oggdec.h
 * Created by: User <Email>
 * Created on: Sat Aug 14 23:24:43 2004
 */

#ifndef _OGGDEC_H_
#define _OGGDEC_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>
#include "exec.h"

void oggdec_add_args(ExecCmd* cmd, gchar* file, gchar** convertedfile);

#endif	/*_OGGDEC_H_*/
