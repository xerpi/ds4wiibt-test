#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include "utils.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
int run = 1;

void button_cb()
{
	if (run != 0) {
		printf(" Exit CB!\n\n");
		run = 0;
	}
}

void print_mac(void *mac)
{
	printf("%02X:%02X:%02X:%02X:%02X:%02X\n", ((u8*)mac)[0], ((u8*)mac)[1], ((u8*)mac)[2], ((u8*)mac)[3], ((u8*)mac)[4], ((u8*)mac)[5]);
}

void init_video()
{
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	printf("\x1b[2;0H");
}

void flip_screen()
{
	VIDEO_WaitVSync();
}
