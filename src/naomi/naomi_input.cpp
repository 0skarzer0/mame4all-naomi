/* NAOMI controls: the cabinet's JVS I/O board read through the KOS MIE
   driver. Tuned for a Crazy Taxi upright (wheel, accelerator, brake, a
   two-way shifter, START), and usable with a stick cabinet:

     START                 1 player start
     brake held + START    a credit, in every game (START is not passed on
                           to the game until it is released)
     coin chutes, SERVICE  coin (SERVICE gives a credit without a coin)
     TEST                  the game's service/test mode switch
     wheel                 analog steering (AD stick X, paddle, dial)
     accelerator / brake   analog pedals (AD stick Y player 1 / player 2),
                           also buttons 1 / 2 when the game wants buttons
     shifter / stick       directions; push buttons 1-4 as buttons 1-4
                           (stick left/right also steer, buttons 1/2 also
                           accelerate/brake); in games with low/high gears
                           the shifter sets the gear: forward high, back low
     START held 3 s        frame rate overlay on/off (top left:
                           frames emulated/shown per second, CPU load %,
                           and the raw JVS values)

   Replaces sdl/input.cpp. */

#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/mie.h>

#include "driver.h"
#include "minimal.h"
#include "naomi.h"
#include <stdio.h>
#include <string.h>

/* Raw JVS values measured on the Crazy Taxi cabinet; the limits widen by
   themselves if a control goes further. */
static int wheel_min = 8448, wheel_centre = 33792, wheel_max = 60160;
static int accel_min = 11776, accel_max = 47104;
static int brake_min = 14080, brake_max = 47360;

static mie_jvs_inputs_t jvs;
static int have_jvs = 0;
static int steer = 0;          /* -128 .. 127 */
static int accel = 0, brake = 0; /* 0 .. 255 */
static int coin_frames = 0;
static int service_held = 0;
static int coin_seen = 0;
static int last_v[3];   /* the last analog port values given to the game */
static int naomi_analog2(int, int, int, int, int, int, int *);
/* Free-spinning wheel (IPT_DIAL): sixteenths of a unit per frame, and the
   wheel angles (0..127 each side) where the two speeds start. */
#ifndef NAOMI_DIAL_SLOW
#define NAOMI_DIAL_SLOW 40
#endif
#ifndef NAOMI_DIAL_FAST
#define NAOMI_DIAL_FAST 96
#endif
#ifndef NAOMI_DIAL_DEAD
#define NAOMI_DIAL_DEAD 14
#endif
#ifndef NAOMI_DIAL_FAST_FROM
#define NAOMI_DIAL_FAST_FROM 105
#endif
static int coins_pending = 0;
static int gear_high = 0;
static uint16_t coin_count[MIE_JVS_COIN_SLOTS];
static int start_held = 0;    /* START was down on the last poll */
static int start_coin = 0;    /* this START press gave a credit: hide it */

static int steer_of(int raw)
{
	if (raw < wheel_min) wheel_min = raw;
	if (raw > wheel_max) wheel_max = raw;
	if (raw < wheel_centre)
		return -((wheel_centre - raw) * 128) / (wheel_centre - wheel_min);
	return ((raw - wheel_centre) * 127) / (wheel_max - wheel_centre);
}

static int pedal_of(int raw, int *lo, int *hi)
{
	if (raw < *lo) *lo = raw;
	if (raw > *hi) *hi = raw;
	int margin = (*hi - *lo) * 6 / 100;
	int from = *lo + margin, to = *hi - margin;
	if (raw <= from) return 0;
	if (raw >= to) return 255;
	return (raw - from) * 255 / (to - from);
}

void naomi_input_init(void)
{
	/* The MIE driver probes the JVS bridge while KOS starts; if that probe
	   missed it (seen in an emulator), probe again now. */
	if (mie_port0_mode() != MIE_PORT0_JVS) {
		mie_shutdown();
		mie_init();
		mie_init_scan();
	}
	have_jvs = 0;
	coin_frames = 0;
}

#ifdef NAOMI_FLYCAST
/* Test builds for the Flycast emulator only: there the MIE driver finds no
   running MIE firmware and does not poll, so the JVS board is read here
   with direct MIE commands (slow: two Maple frames a call). */
extern "C" maple_response_t *mie_cmd_resp(int cmd, void *send_buf, int length);

static int flycast_read(mie_jvs_inputs_t *j)
{
	static const uint8_t req[16] = { 0x17, 0x77, 0, 0, 0, 0, 1, 7, 0x20, 2, 2, 0x21, 2, 0x22, 8, 0 };
	static const uint8_t fetch[4] = { 0x15, 0, 0, 0 };
	maple_response_t *r = mie_cmd_resp(0x86, (void *)req, 4);
	if (!r || (uint8_t)r->response != 0x87)
		return 0;
	r = mie_cmd_resp(0x86, (void *)fetch, 1);
	if (!r || (uint8_t)r->response != 0x87 || r->data_len < 12)
		return 0;
	const uint8_t *d = r->data;
	if (d[0] != 0x16 || d[26] != 1)
		return 0;
	static uint16_t coins = 0;
	memset(j, 0, sizeof(*j));
	j->system.raw = d[27];
	j->p1.raw = (d[28] << 8) | d[29];
	j->p2.raw = (d[30] << 8) | d[31];
	/* Flycast does not count coins for an unknown game: push button 2
	   (keyboard C) is the coin here */
	static int c_held = 0;
	int c = (d[28] & 0x01) != 0;
	if (c && !c_held) coins++;
	c_held = c;
	j->p1.raw &= ~0x0100;
	j->coin[0].count = coins;
	for (int i = 0; i < 8; i++)
		j->analog[i] = (d[38 + 2 * i] << 8) | d[39 + 2 * i];
	return 1;
}
#endif

void naomi_input_poll(void)
{
	maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MIE);
	const mie_state_t *st = dev ? (const mie_state_t *)maple_dev_status(dev) : NULL;
#ifdef NAOMI_FLYCAST
	static mie_state_t fst;
	if (flycast_read(&fst.jvs))
		st = &fst;
#endif
	if (!st) {
		have_jvs = 0;
		return;
	}
	jvs = st->jvs;
	have_jvs = 1;
	steer = steer_of(jvs.wheel);
	accel = pedal_of(jvs.accel, &accel_min, &accel_max);
	brake = pedal_of(jvs.brake, &brake_min, &brake_max);
	/* a stick cabinet: stick left/right steer, push buttons 1/2 pedal */
	if (jvs.p1.left) steer = -128;
	if (jvs.p1.right) steer = 127;
	if (jvs.p1.sw2) accel = 255;
	if (jvs.p1.sw1) brake = 255;

	/* A coin (the slot's meter went up; the driver's one-shot coin_pulse is
	   already cleared in the status) or the SERVICE button, on its press:
	   the coin switch for a few frames, long enough for the game to see it. */
	for (int i = 0; i < MIE_JVS_COIN_SLOTS; i++) {
		uint16_t added = jvs.coin[i].count - coin_count[i];
		if (coin_seen && added > 0 && added < 100)
			coins_pending += added;
		coin_count[i] = jvs.coin[i].count;
	}
	coin_seen = 1;
	if (jvs.p1.up) gear_high = 1;
	if (jvs.p1.down) gear_high = 0;
	int service = jvs.panel.psw.service || jvs.p1.service;
	if (service && !service_held)
		coins_pending++;
	service_held = service;

	/* brake held + START: a credit. That START press is kept from the
	   game until it is released, so it does not also start a race. */
	int start = jvs.p1.start;
	if (start && !start_held && brake > 128) {
		coins_pending++;
		start_coin = 1;
	}
	if (!start)
		start_coin = 0;
	start_held = start;
}

/* START as the game sees it */
static int p1_start(void)
{
	return have_jvs && jvs.p1.start && !start_coin;
}

static int test_pressed(void)
{
	return have_jvs && (jvs.panel.psw.test || jvs.system.test);
}

unsigned long naomi_pad(int njoy)
{
	unsigned long k = 0;
	if (!have_jvs)
		return 0;
	const mie_jvs_player_sw_t &p = njoy <= 1 ? jvs.p1 : jvs.p2;
	if (njoy <= 1 ? p1_start() : p.start) k |= GP2X_START;
	if (p.up) k |= GP2X_UP;
	if (p.down) k |= GP2X_DOWN;
	if (p.left) k |= GP2X_LEFT;
	if (p.right) k |= GP2X_RIGHT;
	if (p.sw2) k |= GP2X_A;
	if (p.sw1) k |= GP2X_B;
	if (p.sw9) k |= GP2X_X;
	if (p.sw10) k |= GP2X_Y;
	return k;
}

void naomi_input_debug(char *out)
{
	/* coin meter, switches, wheel, accelerator, brake (raw JVS), the values
	   given to the game (steering, accelerator, brake), and status: bit 0
	   JVS read, bits 1-2 MIE port mode, bits 4-5 panel test/service */
	sprintf(out, "%04X %04X %04X %04X %04X %02X %02X %02X %02X", jvs.coin[0].count, jvs.p1.raw,
	        jvs.wheel, jvs.accel, jvs.brake, last_v[0] & 0xff, last_v[1] & 0xff, last_v[2] & 0xff,
	        (jvs.panel.psw.raw << 4) | (have_jvs ? 1 : 0) | ((int)mie_port0_mode() << 1));
}

/* For drivers that read the pedals themselves (konamigt). */
int naomi_pedal(int which)
{
	return which == 0 ? accel : brake;
}

/* A gear lever for games with low/high gears, from the cabinet's two-way
   shifter that springs back: forward = high, back = low (latched). */
int naomi_gear(void)
{
	return gear_high;
}

void msdos_init_input(void)
{
	naomi_input_poll();
}

void msdos_shutdown_input(void) { }

void osd_poll_joystick(void)
{
	naomi_input_poll();
	/* START held 3 seconds: frame rate overlay on/off */
	{
		extern int naomi_show_fps;
		static int start_frames = 0;
		if (p1_start()) {
			if (++start_frames == 180)
				naomi_show_fps = !naomi_show_fps;
		} else
			start_frames = 0;
	}
	/* each coin: the switch on for 10 frames, then off for 10 */
	if (coin_frames > 0)
		coin_frames--;
	else if (coins_pending > 0) {
		coins_pending--;
		coin_frames = 20;
	}
}

int osd_key_pressed(int keycode)
{
	if (!have_jvs)
		return 0;
	switch (keycode) {
	case OSD_KEY_1: return p1_start();
	case OSD_KEY_2: return jvs.p2.start;
	case OSD_KEY_3: return coin_frames > 10;
	case OSD_KEY_F2: return test_pressed();
	default: return 0;
	}
}

int osd_joy_pressed(int joycode)
{
	if (!have_jvs)
		return 0;
	const mie_jvs_player_sw_t &p1 = jvs.p1, &p2 = jvs.p2;
	switch (joycode) {
	case OSD_JOY_LEFT:   return p1.left || steer < -64;
	case OSD_JOY_RIGHT:  return p1.right || steer > 64;
	case OSD_JOY_UP:     return p1.up;
	case OSD_JOY_DOWN:   return p1.down;
	case OSD_JOY_FIRE1:  return p1.sw2 || accel > 128;
	case OSD_JOY_FIRE2:  return p1.sw1 || brake > 128;
	case OSD_JOY_FIRE3:  return p1.sw9;
	case OSD_JOY_FIRE4:  return p1.sw10;
	case OSD_JOY_FIRE:   return p1.sw2 || p1.sw1 || p1.sw9 || p1.sw10;
	case OSD_JOY2_LEFT:  return p2.left;
	case OSD_JOY2_RIGHT: return p2.right;
	case OSD_JOY2_UP:    return p2.up;
	case OSD_JOY2_DOWN:  return p2.down;
	case OSD_JOY2_FIRE1: return p2.sw2;
	case OSD_JOY2_FIRE2: return p2.sw1;
	case OSD_JOY2_FIRE3: return p2.sw9;
	case OSD_JOY2_FIRE4: return p2.sw10;
	case OSD_JOY2_FIRE:  return p2.sw2 || p2.sw1 || p2.sw9 || p2.sw10;
	default: return 0;
	}
}

/* Absolute wheel and pedal positions for the driver's analog ports
   (inptport.cpp update_analog_port). Returns 1 when the port is handled.
   reverse: the game's value goes down as the stick goes right/up. */
int naomi_analog(int type, int player, int reverse, int min, int max, int def, int *value)
{
	int r = naomi_analog2(type, player, reverse, min, max, def, value);
	if (r && player < 2) last_v[type == IPT_AD_STICK_Y ? 1 + player : 0] = *value;
	return r;
}

static int naomi_analog2(int type, int player, int reverse, int min, int max, int def, int *value)
{
	if (!have_jvs)
		return 0;
	switch (type) {
	case IPT_DIAL:
		/* A free-spinning wheel (Super Sprint, spinners), played like keys:
		   past a small dead zone the wheel turns at one steady speed whatever
		   the angle; turned near the end of its travel, at a faster one. */
		if (player != 0)
			return 0;
		{
			static int acc = -1;   /* 1/16 units */
			if (acc < 0)
				acc = def << 4;
			int w = reverse ? -steer : steer;
			int a = w < 0 ? -w : w;
			int d = 0;
			if (a > NAOMI_DIAL_FAST_FROM)
				d = NAOMI_DIAL_FAST;
			else if (a > NAOMI_DIAL_DEAD)
				d = NAOMI_DIAL_SLOW;
			acc += w < 0 ? -d : d;
			acc &= 0xfffff;
			*value = acc >> 4;
		}
		return 1;
	case IPT_AD_STICK_X:
	case IPT_PADDLE:
		if (player != 0)
			return 0;
		{
			int w = reverse ? -steer : steer;
			if (w >= 0)
				*value = def + w * (max - def) / 127;
			else
				*value = def + w * (def - min) / 128;
		}
		return 1;
	case IPT_AD_STICK_Y:
		if (player > 1)
			return 0;
		{
			/* a pedal pressed = the stick pushed up */
			int p = player == 0 ? accel : brake;
			int full = reverse ? max : min;
			*value = def + p * (full - def) / 255;
		}
		return 1;
	default:
		return 0;
	}
}

void osd_analogjoy_read(int *analog_x, int *analog_y)
{
	*analog_x = *analog_y = 0;
}

void osd_trak_read(int *deltax, int *deltay)
{
	*deltax = *deltay = 0;
}

void osd_led_w(int led, int on) { (void)led; (void)on; }
