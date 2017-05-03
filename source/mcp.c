/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include "utils.h"
#include <string.h>

#include "prsh.h"
#include "minini.h"

typedef struct {
    u32 unk1;
    char filesystem[10];
    u16 unk2;
    u32 unk3;
    u64 title_id;
    u64 os_id;
} PACKED mcp_launch_region;

mcp_launch_region* mcp_get_launch_region(void)
{
    mcp_launch_region* region = NULL;

    prsh_get_entry("mcp_launch_region", (void*)&region, NULL);
    return region;
}

int mcp_set_launch_title(u64 title_id)
{
    mcp_launch_region* region = mcp_get_launch_region();
    if(!region) return -1;

    region->title_id = title_id;

    return 0;
}

int mcp_get_launch_title(u64* title_id)
{
    mcp_launch_region* region = mcp_get_launch_region();
    if(!region) return -1;

    *title_id = region->title_id;

    return 0;
}

int mcp_ini(const char* key, const char* value)
{
    if(!strcmp(key, "launch_title"))
    {
        u64 title_id = 0;
        mcp_get_launch_title(&title_id);

        title_id = minini_get_uint(value, title_id);
        mcp_set_launch_title(title_id);
    }

    return 0;
}
