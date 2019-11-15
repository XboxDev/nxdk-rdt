#include <hal/video.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/debug.h>

#include "net.h"
#include "gfx.h"
#include "dbgd.h"

/* Main program function */
void main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    gfx_init();
    pb_print("NXDK Development Dash\n");

    pb_show_debug_screen();
    net_init();
    dbgd_init();

    while (1) NtYieldExecution();
    /* Not ready for below yet, just stick to debug screen for now */

    pb_show_front_screen();

    while(1) {
        gfx_begin();
        gfx_draw_bg();
        pb_draw_text_screen();
        gfx_end();
        gfx_present();
        NtYieldExecution();
    }

    pb_show_debug_screen();
    pb_kill();
}
