#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "ds4wiibt.h"

void conn_cb(void *usrdata)
{
	int *i = (int *)usrdata;
	printf("Controller connected: %i\n", *i);
}

void discon_cb(void *usrdata)
{
	int *i = (int *)usrdata;
	printf("Controller disconnected: %i\n", *i);
}

int main(int argc, char *argv[])
{
	SYS_SetResetCallback(button_cb);
	SYS_SetPowerCallback(button_cb);
    WPAD_Init();
    
	init_video();
	printf("ds4wiibt by xerpi\n");
	
	struct bd_addr addr;
	//PUT YOUR DS4 MAC HERE
	BD_ADDR(&addr, 0xC5, 0x18, 0x2A, 0x6D, 0x66, 0x1C);	
	
	int i = 5;
	struct ds4wiibt_context ctx;
	ds4wiibt_initialize(&ctx);
	ds4wiibt_set_user_data(&ctx, &i);
	ds4wiibt_set_connect_cb(&ctx, conn_cb);
	ds4wiibt_set_disconnect_cb(&ctx, discon_cb);
	ds4wiibt_connect(&ctx, &addr);

	printf("MAIN LOOP\n");
	while (run) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) run = 0;
		usleep(16666);
		flip_screen();
	}
	ds4wiibt_close(&ctx);
	return 0;
}

