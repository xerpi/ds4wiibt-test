#include <stdio.h>
#include <unistd.h>
#include <gccore.h>
#include <fat.h>
#include <wiiuse/wpad.h>
#include "ds4wiibt.h"
#include "utils.h"
#include "ds4wiibt_config.h"

static void print_data(struct ds4wiibt_input *inp);
static void conn_cb(void *usrdata);
static void discon_cb(void *usrdata);

int main(int argc, char *argv[])
{
	fatInitDefault();
	WPAD_Init();
	init_video();
	SYS_SetResetCallback(button_cb);
	SYS_SetPowerCallback(button_cb);
	printf("ds4wiibt by xerpi\n");

	struct ds4wiibt_config_ctx conf;
	ds4wiibt_config_initialize(&conf);
	
	if (!ds4wiibt_config_read(DS4WIIBT_CONFIG_FILE, &conf)) {
		printf("Could not open config file.\n");
		printf("Make sure to run ds4wii pair tool first.\n");
		while (run) {
			WPAD_ScanPads();
			u32 pressed = WPAD_ButtonsDown(0);
			if (pressed & WPAD_BUTTON_HOME) run = 0;
			flip_screen();
		}
		return 0;
	}
	printf("Read %i DS4 MACs\n", conf.hdr.n_devices);
	struct bd_addr addr, addr_rev;
	//We need to reverse the MAC address
	addr_rev.addr[0] =  conf.MAC_list[0][5];
	addr_rev.addr[1] =  conf.MAC_list[0][4];
	addr_rev.addr[2] =  conf.MAC_list[0][3];
	addr_rev.addr[3] =  conf.MAC_list[0][2];
	addr_rev.addr[4] =  conf.MAC_list[0][1];
	addr_rev.addr[5] =  conf.MAC_list[0][0];

	struct ds4wiibt_context ctx;
	ds4wiibt_initialize(&ctx);
	ds4wiibt_set_user_data(&ctx, NULL);
	ds4wiibt_set_connect_cb(&ctx, conn_cb);
	ds4wiibt_set_disconnect_cb(&ctx, discon_cb);

	LOG("Connecting to: ");
	print_mac((struct bd_addr*)conf.MAC_list[0]);
	ds4wiibt_connect(&ctx, &addr_rev);
	printf("Listening for an incoming connection...\n");

	while (run) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) run = 0;
		if (is_connected(&ctx)) {
			print_data(&ctx.input);
			if (ctx.input.PS && ctx.input.OPTIONS) run = 0;
		}
		flip_screen();
	}
	ds4wiibt_close(&ctx);
	ds4wiibt_config_free(&conf);
	return 0;
}

static void conn_cb(void *usrdata)
{
	printf("Controller connected.\n");
}

static void discon_cb(void *usrdata)
{
	printf("Controller disconnected.\n");
}

static void print_data(struct ds4wiibt_input *inp)
{ 
    printf("\x1b[10;0H");
    printf("PS: %1i   OPTIONS: %1i  SHARE: %1i   /\\: %1i   []: %1i   O: %1i   X: %1i\n", \
            inp->PS, inp->OPTIONS, inp->SHARE, inp->triangle, \
            inp->square, inp->circle, inp->cross);
            
    printf("TPAD: %1i   L3: %1i   R3: %1i\n", \
            inp->TPAD, inp->L3, inp->R3);

    printf("L1: %1i   L2: %1i   R1: %1i   R2: %1i   DPAD: %1i\n", \
            inp->L1, inp->L2, inp->R1, inp->R2, \
            inp->dpad);
    printf("LX: %2X   LY: %2X   RX: %2X   RY: %2X  battery: %1X\n", \
            inp->leftX, inp->leftY, inp->rightX, inp->rightY, inp->battery);
    
    printf("headphones: %1X   microphone: %1X   usb_plugged: %1X  batt_level: %2X\n", \
            inp->headphones, inp->microphone, inp->cable, inp->batt_level);    

    printf("aX: %5hi  aY: %5hi  aZ: %5hi  roll: %5hi  yaw: %5hi  pitch: %5hi\n", \
            inp->accelX, inp->accelY, inp->accelZ, inp->roll, inp->yaw, inp->pitch);

    printf("Ltrigger: %2X   Rtrigger: %2X  trackpadpackets: %4i  packetcnt: %4i\n", \
            inp->triggerL, inp->triggerR, inp->trackpad_pkts, inp->packet_count);
            
    printf("f1active: %2X   f1ID: %2X  f1X: %4i  f1Y: %4i\n", \
            inp->finger1.active, inp->finger1.ID, inp->finger1.X, inp->finger1.Y);
    printf("f2active: %2X   f2ID: %2X  f2X: %4i  f2Y: %4i\n", \
            inp->finger2.active, inp->finger2.ID, inp->finger2.X, inp->finger2.Y);

}
