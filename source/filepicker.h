/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _FILEPICKER_H
#define _FILEPICKER_H

#include "types.h"

#include "console.h"
#include "ff.h"

typedef struct {
    char path[_MAX_LFN + 1];
    char directory[256][256];
    int directories;
    char file[256][256];
    int files;
    bool folderpick;
    int selected;
    bool update_needed;
    int scroll_x;
    int show_y;
} picker;

char* pick_file(char* path, bool folderpick, char* filename_buf);

#endif
