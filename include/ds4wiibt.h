#ifndef DS4WIIBT_H
#define DS4WIIBT_H

#include <stdio.h>
#include <gccore.h>
#include <unistd.h>
#include <bte/bte.h>
#include "utils.h"
#include "btpbuf.h"
#include "l2cap.h"
#include "hci.h"

#undef LOG
#define LOG printf

#define SDP_PSM 0x01

typedef void (*ds4wiibt_cb)(void *usrdata);

enum ds4wiibt_status {
	DS4WIIBT_STATUS_DISCONNECTED,
	DS4WIIBT_STATUS_CONNECTED,
};

struct ds4wiibt_context {
	struct l2cap_pcb *sdp_pcb;
	struct l2cap_pcb *ctrl_pcb;
	struct l2cap_pcb *data_pcb;
	struct bd_addr	  bdaddr;
	struct {
		unsigned char r, g, b;
		unsigned char on, off;
	} led;
	struct {
		unsigned char right, left;
	} rumble;
	void *usrdata;
	ds4wiibt_cb connect_cb;
	ds4wiibt_cb disconnect_cb;
	enum ds4wiibt_status status;
};

void ds4wiibt_initialize(struct ds4wiibt_context *ctx);
void ds4wiibt_set_user_data(struct ds4wiibt_context *ctx, void *data);
void ds4wiibt_set_connect_cb(struct ds4wiibt_context *ctx, ds4wiibt_cb cb);
void ds4wiibt_set_disconnect_cb(struct ds4wiibt_context *ctx, ds4wiibt_cb cb);
void ds4wiibt_set_led_rgb(struct ds4wiibt_context *ctx, u8 red, u8 green, u8 blue);
void ds4wiibt_set_led_blink(struct ds4wiibt_context *ctx, u8 time_on, u8 time_off);
void ds4wiibt_set_rumble(struct ds4wiibt_context *ctx, u8 right_motor, u8 left_motor);
void ds4wiibt_send_ledsrumble(struct ds4wiibt_context *ctx);
void ds4wiibt_connect(struct ds4wiibt_context *ctx, struct bd_addr *addr);
void ds4wiibt_close(struct ds4wiibt_context *ctx);

#define is_connected(ctxp) \
	(ctxp->status == DS4WIIBT_STATUS_CONNECTED)

#endif
