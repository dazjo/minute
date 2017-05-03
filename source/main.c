/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2009          Andre Heider "dhewg" <dhewg@wiibrew.org>
 *  Copyright (C) 2009          John Kelley <wiidev@kelley.ca>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "main.h"

#include "types.h"
#include "utils.h"
#include "latte.h"
#include "sdcard.h"
#include "mlc.h"
#include "string.h"
#include "memory.h"
#include "gfx.h"
#include "elm.h"
#include "irq.h"
#include "exception.h"
#include "crypto.h"
#include "nand.h"
#include "sdhc.h"
#include "dump.h"
#include "isfs.h"
#include "smc.h"
#include "filepicker.h"
#include "ancast.h"
#include "minini.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "ppc_elf.h"
#include "ppc.h"

static struct {
    int mode;
    u32 vector;
} boot = {0};

bool autoboot = false;
u32 autoboot_timeout_s = 3;
char autoboot_file[256] = "ios.img";

int main_autoboot(void);

u32 _main(void *base)
{
    (void)base;
    int res = 0; (void)res;

    gfx_clear(GFX_ALL, BLACK);
    printf("minute loading\n");

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    irq_initialize();
    printf("Interrupts initialized\n");

    srand(read32(LT_TIMER));
    crypto_initialize();
    printf("crypto support initialized\n");

    printf("Initializing SD card...\n");
    sdcard_init();

    printf("Mounting SD card...\n");
    res = ELM_Mount();
    if(res) {
        printf("Error while mounting SD card (%d).\n", res);
        panic(0);
    }

    minini_init();
    
    printf("Initializing MLC...\n");
    mlc_init();

    if(mlc_check_card() == SDMMC_NO_CARD) {
        printf("Error while initializing MLC.\n");
        panic(0);
    }
    mlc_ack_card();

    printf("Mounting SLC...\n");
    isfs_init();
    
    // Prompt user to skip autoboot, time = 0 will skip this.
    if(autoboot)
    {
        while((autoboot_timeout_s-- > 0) && autoboot)
        {
            printf("Autobooting in %d seconds...\n", (int)autoboot_timeout_s + 1);
            printf("Press the POWER button or EJECT button to skip autoboot.\n");
            for(u32 i = 0; i < 1000000; i += 100000)
            {
                // Get input at .1s intervals.
                u8 input = smc_get_events();
                udelay(100000);
                if((input & SMC_EJECT_BUTTON) || (input & SMC_POWER_BUTTON))
                    autoboot = false;
            }
        }
    }
    
    // Try to autoboot if specified, if it fails just load the menu.
    if(autoboot && main_autoboot() == 0)
        printf("Autobooting...\n");
    else
    {
        smc_get_events();
        smc_set_odd_power(false);

        menu_init(&menu_main);

        smc_get_events();
        smc_set_odd_power(true);
    }
    
    printf("Unmounting SLC...\n");
    isfs_fini();

    printf("Shutting down MLC...\n");
    mlc_exit();
    
    printf("Shutting down SD card...\n");
    ELM_Unmount();
    sdcard_exit();

    printf("Shutting down interrupts...\n");
    irq_shutdown();

    printf("Shutting down caches and MMU...\n");
    mem_shutdown();

    switch(boot.mode) {
        case 0:
            if(boot.vector) {
                printf("Vectoring to 0x%08lX...\n", boot.vector);
            } else {
                printf("No vector address, hanging!\n");
                panic(0);
            }
            break;
        case 1: smc_power_off(); break;
        case 2: smc_reset(); break;
    }

    return boot.vector;
}

int boot_ini(const char* key, const char* value)
{
    if(!strcmp(key, "autoboot"))
        autoboot = minini_get_bool(value, 0);
    if(!strcmp(key, "autoboot_file"))
        strncpy(autoboot_file, value, sizeof(autoboot_file));
    if(!strcmp(key, "autoboot_timeout"))
        autoboot_timeout_s = (u32)minini_get_uint(value, 3);
    
    return 0;
}

int main_autoboot(void)
{
    FILE* f = fopen(autoboot_file, "rb");
    if(f == NULL)
    {
        printf("Failed to open %s.\n", autoboot_file);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
        return -1;
    }

    u32 magic;
    fread(&magic, 1, sizeof(magic), f);
    fclose(f);
    
    // Ancast image.
    if(magic == 0xEFA282D9)
        boot.vector = ancast_iop_load(autoboot_file);
    
    if(boot.vector)
    {
        boot.mode = 0;
        return 0;
    }
    else
    {
        printf("Failed to load file for autoboot: %s\n", autoboot_file);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
        return -2;
    }
}

void main_reload(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("fw.img");

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load fw.img!\n");
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_shutdown(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 1;
    menu_active = false;
}

void main_reset(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 2;
    menu_active = false;
}

void main_boot_ppc(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char path[_MAX_LFN] = {0};
    pick_file("sdmc:", false, path);

    u32 entry = 0;
    int res = ppc_load_file(path, &entry);
    if(res) {
        printf("ppc_load_file: %d\n", res);
        goto ppc_exit;
    }

    ppc_jump(entry);

ppc_exit:
    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_quickboot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("ios.img");

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load 'ios.img'!\n");
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_boot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char path[_MAX_LFN] = {0};
    pick_file("sdmc:", false, path);

    boot.vector = ancast_iop_load(path);

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load '%s'!\n", path);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_reset_crash(void)
{
	gfx_clear(GFX_ALL, BLACK);

	printf("Clearing SMC crash buffer...\n");

	const char buffer[64 + 1] = "Crash buffer empty.";
	smc_set_panic_reason(buffer);

    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_get_crash(void)
{
    gfx_clear(GFX_ALL, BLACK);
    printf("Reading SMC crash buffer...\n");

    char buffer[64 + 1] = {0};
    smc_get_panic_reason(buffer);

    // We use this SMC buffer for storing exception info, however, it is only 64 bytes.
    // This is exactly enough for r0-r15, but nothing else - not even some exception "magic"
    // or even exception type. Here we have some crap "heuristic" to determine if it's ASCII text
    // (a panic reason) or an exception dump.
    bool exception = false;
    for(int i = 0; i < 64; i++)
    {
        char c = buffer[i];
        if(c >= 32 && c < 127) continue;
        if(c == 10 || c == 0) continue;

        exception = true;
        break;
    }

    if(exception) {
        u32* regs = (u32*)buffer;
        printf("Exception registers:\n");
        printf("  R0-R3: %08lx %08lx %08lx %08lx\n", regs[0], regs[1], regs[2], regs[3]);
        printf("  R4-R7: %08lx %08lx %08lx %08lx\n", regs[4], regs[5], regs[6], regs[7]);
        printf(" R8-R11: %08lx %08lx %08lx %08lx\n", regs[8], regs[9], regs[10], regs[11]);
        printf("R12-R15: %08lx %08lx %08lx %08lx\n", regs[12], regs[13], regs[14], regs[15]);
    } else {
        printf("Panic reason:\n");
        printf("%s\n", buffer);
    }

    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_credits(void)
{
    gfx_clear(GFX_ALL, BLACK);
    console_init();

    console_add_text("minute (not minute) - a Wii U port of mini\n");

    console_add_text("The SALT team: Dazzozo, WulfyStylez, shinyquagsire23 and Relys (in spirit)\n");

    console_add_text("Special thanks to fail0verflow (formerly Team Twiizers) for the original \"mini\", and for the vast\nmajority of Wii research and early Wii U research!\n");

    console_add_text("Thanks to all WiiUBrew contributors, including: Hykem, Marionumber1, smea, yellows8, derrek,\nplutoo, naehrwert...\n");

    console_add_text("Press POWER to exit.");

    console_show();
    smc_wait_events(SMC_POWER_BUTTON);
}
