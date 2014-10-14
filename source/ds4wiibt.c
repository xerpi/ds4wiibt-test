#include <stdio.h>
#include <unistd.h>
#include <ogc/machine/processor.h>
#include <bte/bte.h>
#include "ds4wiibt.h"
#include "hci.h"
#include "btpbuf.h"

static int senddata_raw(struct l2cap_pcb *pcb, void *message, u16 len);
static int set_operational(struct ds4wiibt_context *ctx);
static int send_output_report(struct ds4wiibt_context *ctx);
static void correct_input(struct ds4wiibt_input *inp);

static err_t l2ca_connect_cfm_cb_ctrl_data(void *arg, struct l2cap_pcb *pcb, u16 result, u16 status);
static err_t l2ca_connect_ind_cb_sdp(void *arg, struct l2cap_pcb *pcb, err_t err);

static err_t l2ca_disconnect_ind_cb(void *arg, struct l2cap_pcb *pcb, err_t err);
static err_t l2ca_disconnect_cfm_cb(void *arg, struct l2cap_pcb *pcb);
static err_t l2ca_timeout_ind_cb(void *arg, struct l2cap_pcb *pcb, err_t err);
static err_t l2ca_recv_cb(void *arg, struct l2cap_pcb *pcb, struct pbuf *p, err_t err);

static err_t pin_req_cb(void *arg, struct bd_addr *bdaddr);
static err_t auth_complete_cb(void *arg, struct bd_addr *bdaddr);
static err_t link_key_req_cb(void *arg, struct bd_addr *bdaddr);

void ds4wiibt_initialize(struct ds4wiibt_context *ctx)
{
	ctx->sdp_pcb  = NULL;
	ctx->ctrl_pcb = NULL;
	ctx->data_pcb = NULL;
	memset(&ctx->bdaddr, 0, sizeof(ctx->bdaddr));
	memset(&ctx->input,  0, sizeof(ctx->input));
	ds4wiibt_set_led_rgb(ctx, 0x00, 0x00, 0xFF);
	ds4wiibt_set_led_blink(ctx, 0xFF, 0x00);
	ds4wiibt_set_rumble(ctx, 0x00, 0x00);
	ctx->usrdata = NULL;
	ctx->connect_cb = NULL;
	ctx->disconnect_cb = NULL;
	ctx->status = DS4WIIBT_STATUS_DISCONNECTED;
}

void ds4wiibt_set_user_data(struct ds4wiibt_context *ctx, void *data)
{
	ctx->usrdata = data;
}

void ds4wiibt_set_connect_cb(struct ds4wiibt_context *ctx, ds4wiibt_cb cb)
{
	ctx->connect_cb = cb;
}

void ds4wiibt_set_disconnect_cb(struct ds4wiibt_context *ctx, ds4wiibt_cb cb)
{
	ctx->disconnect_cb = cb;
}

void ds4wiibt_set_led_rgb(struct ds4wiibt_context *ctx, u8 red, u8 green, u8 blue)
{
	ctx->led.r = red;
	ctx->led.g = green;
	ctx->led.b = blue;
}

void ds4wiibt_set_led_blink(struct ds4wiibt_context *ctx, u8 time_on, u8 time_off)
{
	ctx->led.on  = time_on;
	ctx->led.off = time_off;
}

void ds4wiibt_set_rumble(struct ds4wiibt_context *ctx, u8 right_motor, u8 left_motor)
{
	ctx->rumble.right = right_motor;
	ctx->rumble.left = left_motor;
}

void ds4wiibt_send_ledsrumble(struct ds4wiibt_context *ctx)
{
	if (is_connected(ctx))
		send_output_report(ctx);
}

void ds4wiibt_connect(struct ds4wiibt_context *ctx, struct bd_addr *addr)
{
	u32 level = IRQ_Disable();
	
	bd_addr_set(&ctx->bdaddr, addr);
	
	ctx->sdp_pcb  = l2cap_new();
	ctx->ctrl_pcb = l2cap_new();
	ctx->data_pcb = l2cap_new();
	
	l2cap_arg(ctx->sdp_pcb,  ctx);
	l2cap_arg(ctx->ctrl_pcb, ctx);
	l2cap_arg(ctx->data_pcb, ctx);
	
	l2cap_disconnect_ind(ctx->sdp_pcb,  l2ca_disconnect_ind_cb);
	l2cap_disconnect_ind(ctx->ctrl_pcb, l2ca_disconnect_ind_cb);
	l2cap_disconnect_ind(ctx->data_pcb, l2ca_disconnect_ind_cb);
	
	l2cap_timeout_ind(ctx->sdp_pcb,  l2ca_timeout_ind_cb);
	l2cap_timeout_ind(ctx->ctrl_pcb, l2ca_timeout_ind_cb);
	l2cap_timeout_ind(ctx->data_pcb, l2ca_timeout_ind_cb);
	
	l2cap_recv(ctx->sdp_pcb,  l2ca_recv_cb);
	l2cap_recv(ctx->ctrl_pcb, l2ca_recv_cb);
	l2cap_recv(ctx->data_pcb, l2ca_recv_cb);

	hci_link_key_req(link_key_req_cb);
	
	l2cap_connect_ind(ctx->sdp_pcb, &ctx->bdaddr, SDP_PSM, l2ca_connect_ind_cb_sdp);

	IRQ_Restore(level);
}

void ds4wiibt_close(struct ds4wiibt_context *ctx)
{
	//Turn OFF the LED and Rumble
	ds4wiibt_set_led_blink(ctx, 0x00, 0xFF);
	ds4wiibt_set_rumble(ctx, 0x00, 0x00);
	ds4wiibt_send_ledsrumble(ctx);
	if (is_connected(ctx)) {
		if (ctx->sdp_pcb)  l2ca_disconnect_req(ctx->sdp_pcb,  l2ca_disconnect_cfm_cb);
		if (ctx->ctrl_pcb) l2ca_disconnect_req(ctx->ctrl_pcb, l2ca_disconnect_cfm_cb);
		if (ctx->data_pcb) l2ca_disconnect_req(ctx->data_pcb, l2ca_disconnect_cfm_cb);
	} else {
		if (ctx->sdp_pcb)  l2cap_close(ctx->sdp_pcb);
		if (ctx->ctrl_pcb) l2cap_close(ctx->ctrl_pcb);
		if (ctx->data_pcb) l2cap_close(ctx->data_pcb);
		ctx->sdp_pcb  = NULL;
		ctx->ctrl_pcb = NULL;
		ctx->data_pcb = NULL;
	}
	while (is_connected(ctx)) usleep(50);
}

static err_t l2ca_recv_cb(void *arg, struct l2cap_pcb *pcb, struct pbuf *p, err_t err)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL || err != ERR_OK) return -1;

	u8 *rd = p->payload;
	
	//LOG("RECV, PSM: %i  len: %i |", l2cap_psm(pcb), p->len);
	//LOG(" 0x%X  0x%X  0x%X  0x%X  0x%X\n", rd[0], rd[1], rd[2], rd[3], rd[4]);
	
	switch (l2cap_psm(pcb))	{
	case SDP_PSM:
		break;
	case HIDP_PSM:
		break;
	case INTR_PSM:
		switch (rd[1]) {
		case 0x11: //Full report
			memcpy(&ctx->input, rd + 4, sizeof(ctx->input));
			correct_input(&ctx->input);
			break;
		case 0x01: //Short report
			memcpy(&ctx->input, rd + 4, 9);
			break;
		}
		break;
	}

	return ERR_OK;
}

static err_t l2ca_connect_cfm_cb_ctrl_data(void *arg, struct l2cap_pcb *pcb, u16 result, u16 status)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL) return -1;
	
	LOG("l2ca_connect_cfm_cb, PSM: %i\n", l2cap_psm(pcb));
	
	switch (l2cap_psm(pcb))	{
	case HIDP_PSM:
		/* Control PSM is connected, request a Data PSM connection */
		set_operational(ctx);
		send_output_report(ctx);
		l2ca_connect_req(ctx->data_pcb, &ctx->bdaddr, INTR_PSM, 0, l2ca_connect_cfm_cb_ctrl_data);
		break;
	case INTR_PSM:
		/* All the PSM are connected! */
		if (ctx->connect_cb != NULL) {
			ctx->status = DS4WIIBT_STATUS_CONNECTED;
			//Notify the user
			ctx->connect_cb(ctx->usrdata);
		}
		break;
	}

	return ERR_OK;
}

static err_t l2ca_connect_ind_cb_sdp(void *arg, struct l2cap_pcb *pcb, err_t err)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL) return -1;
	
	LOG("l2ca_connect_ind_cb_sdp, PSM: %i\n", l2cap_psm(pcb));
	
	/* Auth successful, connect to the Control and Data PSM */
	if (l2cap_psm(pcb) == SDP_PSM) {
		l2ca_connect_req(ctx->ctrl_pcb, &ctx->bdaddr, HIDP_PSM, 0, l2ca_connect_cfm_cb_ctrl_data);
	}
	
	return ERR_OK;
}

static err_t l2ca_disconnect_ind_cb(void *arg, struct l2cap_pcb *pcb, err_t err)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL) return -1;
	
	LOG("l2ca_disconnect_ind_cb, PSM: %i\n", l2cap_psm(pcb));
	
	switch (l2cap_psm(pcb))	{
	case SDP_PSM:
		//Is SDP PSM disconnection possible here?
		break;
	case HIDP_PSM:
		l2cap_close(ctx->ctrl_pcb);
		ctx->ctrl_pcb = NULL;
		break;
	case INTR_PSM:
		l2cap_close(ctx->data_pcb);
		ctx->data_pcb = NULL;
		break;
	}
	
	//If the Data and Control PSMs are disconnected, listen for an incoming connection again
	if ((ctx->ctrl_pcb == NULL) && (ctx->data_pcb == NULL)) {
		
		ctx->status = DS4WIIBT_STATUS_DISCONNECTED;
		if (ctx->disconnect_cb != NULL) ctx->disconnect_cb(ctx->usrdata);
		
		ctx->ctrl_pcb = l2cap_new();
		ctx->data_pcb = l2cap_new();
		
		l2cap_arg(ctx->ctrl_pcb, ctx);
		l2cap_arg(ctx->data_pcb, ctx);
		
		l2cap_disconnect_ind(ctx->ctrl_pcb, l2ca_disconnect_ind_cb);
		l2cap_disconnect_ind(ctx->data_pcb, l2ca_disconnect_ind_cb);
		
		l2cap_timeout_ind(ctx->ctrl_pcb, l2ca_timeout_ind_cb);
		l2cap_timeout_ind(ctx->data_pcb, l2ca_timeout_ind_cb);

		l2cap_recv(ctx->ctrl_pcb, l2ca_recv_cb);
		l2cap_recv(ctx->data_pcb, l2ca_recv_cb);
	}

	return ERR_OK;
}

static err_t l2ca_disconnect_cfm_cb(void *arg, struct l2cap_pcb *pcb)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL) return -1;
	
	LOG("l2ca_disconnect_cfm_cb, PSM: %i\n", l2cap_psm(pcb));
	
	switch (l2cap_psm(pcb))	{
	case SDP_PSM:
		l2cap_close(ctx->sdp_pcb);
		ctx->sdp_pcb = NULL;
		break;
	case HIDP_PSM:
		l2cap_close(ctx->ctrl_pcb);
		ctx->ctrl_pcb = NULL;
		break;
	case INTR_PSM:
		l2cap_close(ctx->data_pcb);
		ctx->data_pcb = NULL;
		break;
	}
	//User has finished it, so don't listen again
	if ((ctx->sdp_pcb == NULL) && (ctx->ctrl_pcb == NULL) && (ctx->data_pcb == NULL)) {
		ctx->status = DS4WIIBT_STATUS_DISCONNECTED;
		if (ctx->disconnect_cb != NULL) ctx->disconnect_cb(ctx->usrdata);
	}

	return ERR_OK;
}

static err_t l2ca_timeout_ind_cb(void *arg, struct l2cap_pcb *pcb, err_t err)
{
	struct ds4wiibt_context *ctx = (struct ds4wiibt_context *)arg;
	if (ctx == NULL || pcb == NULL) return -1;
	
	LOG("l2ca_timeout_ind_cb, PSM: %i\n", l2cap_psm(pcb));

	//Disconnect?
	switch (l2cap_psm(pcb))	{
	case SDP_PSM:
		break;
	case HIDP_PSM:
		break;
	case INTR_PSM:
		break;
	}

	return ERR_OK;
}

static err_t pin_req_cb(void *arg, struct bd_addr *bdaddr)
{
	LOG("pin_req cb!\n");
	return ERR_OK;
}
static err_t auth_complete_cb(void *arg, struct bd_addr *bdaddr)
{
	LOG("auth_complete_cb!\n");
	return ERR_OK;
}
static err_t link_key_req_cb(void *arg, struct bd_addr *bdaddr)
{
	LOG("link_key_req_cb!\n");
	static unsigned char key[16] = {0};
	return hci_link_key_req_reply(bdaddr, key);
}

static int senddata_raw(struct l2cap_pcb *pcb, void *message, u16 len)
{
	err_t err;
	struct pbuf *p;

	if ((pcb == NULL) || (message==NULL) || (len==0)) return ERR_VAL;

	if ((p = btpbuf_alloc(PBUF_RAW, (len), PBUF_RAM)) == NULL) {
		LOG("bte_senddata_raw: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	//((u8*)p->payload)[0] = (HIDP_TRANS_DATA|HIDP_DATA_RTYPE_OUPUT);
	memcpy(p->payload, message, len);

	err = l2ca_datawrite(pcb, p);
	btpbuf_free(p);

	return err;
}

static void correct_input(struct ds4wiibt_input *inp)
{
	inp->gyroX  = bswap16(inp->gyroX);
	inp->gyroY  = bswap16(inp->gyroY);
	inp->gyroZ  = bswap16(inp->gyroZ);
	inp->accelX = bswap16(inp->accelX);
	inp->accelY = bswap16(inp->accelY);
	inp->accelZ = bswap16(inp->accelZ);

	register u8 *p8 = (u8*)&inp->finger1 + 1;
	inp->finger1.X = p8[0] | (p8[1]&0xF)<<8;
	inp->finger1.Y = ((p8[1]&0xF0)>>4) | (p8[2]<<4);
	inp->finger1.active = !inp->finger1.active;
	p8 += 4;
	inp->finger2.X = p8[0] | (p8[1]&0xF)<<8;
	inp->finger2.Y = ((p8[1]&0xF0)>>4) | (p8[2]<<4);
	inp->finger2.active = !inp->finger2.active;
}

static int set_operational(struct ds4wiibt_context *ctx)
{
	unsigned char buf[38] = {
		(HIDP_TRANS_DATA | HIDP_DATA_RTYPE_FEATURE),
		0x02
	};
	return senddata_raw(ctx->ctrl_pcb, buf, sizeof(buf));
}

static int send_output_report(struct ds4wiibt_context *ctx)
{
	unsigned char buf[79] = {
		(HIDP_TRANS_SETREPORT | HIDP_DATA_RTYPE_OUPUT),
		0x11,
		0xB0,
		0x00,
		0x0F
	};
	buf[7]  = ctx->rumble.right;
	buf[8]  = ctx->rumble.left;
	buf[9]  = ctx->led.r;
	buf[10] = ctx->led.g;
	buf[11] = ctx->led.b;
	buf[12] = ctx->led.on;
	buf[13] = ctx->led.off;
	return senddata_raw(ctx->ctrl_pcb, buf, sizeof(buf));
}

