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

typedef struct {
    char name[0x100];
    void* data;
    u32 size;
    u8 unk[0x24];
} PACKED prsh_entry;

typedef struct {
    u32 unk1;
    u32 magic;
    u32 unk2;
    u32 size;
    u8 unk3[8];
    u32 entries;
    prsh_entry entry[];
} PACKED prsh_header;

static prsh_header* header = NULL;
static bool initialized = false;

void prsh_init(void)
{
    if(initialized) return;

    void* buffer = (void*)0x10000400;
    size_t size = 0x7C00;
    while(size) {
        if(!memcmp(buffer, "PRSH", sizeof(u32))) break;
        buffer += sizeof(u32);
        size -= sizeof(u32);
    }

    header = buffer - sizeof(u32);
    initialized = true;
}

int prsh_get_entry(const char* name, void** data, size_t* size)
{
    prsh_init();
    if(!name) return -1;

    for(int i = 0; i < header->entries; i++) {
        prsh_entry* entry = &header->entry[i];

        if(!strncmp(name, entry->name, sizeof(entry->name))) {
            if(data) *data = entry->data;
            if(size) *size = entry->size;
            return 0;
        }
    }

    return -2;
}
