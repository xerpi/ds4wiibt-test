#ifndef DS4WIIBT_H
#define DS4WIIBT_H

#include <gccore.h>
#include "l2cap.h"

#undef LOG
#define LOG printf

#define SDP_PSM 0x01

typedef void (*ds4wiibt_cb)(void *usrdata);

enum ds4wiibt_status {
	DS4WIIBT_STATUS_DISCONNECTED,
	DS4WIIBT_STATUS_CONNECTED,
};

struct ds4wiibt_input {
	unsigned char leftX;
	unsigned char leftY;
	unsigned char rightX;
	unsigned char rightY;
	struct {
		unsigned char triangle : 1;
		unsigned char circle   : 1;
		unsigned char cross    : 1;
		unsigned char square   : 1;
		unsigned char dpad     : 4;
	};
	struct {
		unsigned char R3      : 1;
		unsigned char L3      : 1;
		unsigned char OPTIONS : 1;
		unsigned char SHARE   : 1;
		unsigned char R2      : 1;
		unsigned char L2      : 1;
		unsigned char R1      : 1;
		unsigned char L1      : 1;
	};
	struct {
		unsigned char counter : 6;
		unsigned char TPAD    : 1;
		unsigned char PS      : 1;
	};
	unsigned char triggerL;
	unsigned char triggerR;
	unsigned char timestamp[2];
	unsigned char battery;
	struct {
		short accelX;
		short accelY;
		short accelZ;
		union {
			short gyroZ;
			short roll;
		};
		union {
			short gyroY;
			short yaw;
		};
		union {
			short gyroX;
			short pitch;
		};
	};
	unsigned char unk1[5];
	struct {
		unsigned char unused     : 1;
		unsigned char microphone : 1;
		unsigned char headphones : 1;
		unsigned char cable      : 1;
		unsigned char batt_level : 4;
	};
	unsigned char unk2[2];
	unsigned char trackpad_pkts;
	unsigned char packet_count;
	struct {
		unsigned int active : 1;
		unsigned int ID     : 7;
		unsigned int X      : 12;
		unsigned int Y      : 12;
	} finger1;
	struct {
		unsigned int active : 1;
		unsigned int ID     : 7;
		unsigned int X      : 12;
		unsigned int Y      : 12;
	} finger2;
} __attribute__((packed, aligned(32)));

struct ds4wiibt_context {
	struct l2cap_pcb *sdp_pcb;
	struct l2cap_pcb *ctrl_pcb;
	struct l2cap_pcb *data_pcb;
	struct bd_addr	  bdaddr;
	struct ds4wiibt_input input;
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
	((ctxp)->status == DS4WIIBT_STATUS_CONNECTED)

#endif
