/* NAOMI platform layer for MAME4ALL (KallistiOS, KOS_SUBARCH=naomi). */
#ifndef NAOMI_PLATFORM_H
#define NAOMI_PLATFORM_H

#include <stdint.h>

/* naomi_video.cpp: 8-bit pixels through gp2x_sdlwrapper_palette into a PVR
   texture, drawn over the whole 640x480 screen. */
void naomi_video_init(void);
void naomi_present(const unsigned char *src, int pitch, int width, int height);

/* naomi_audio.cpp */
void naomi_audio_quit(void);

/* naomi_input.cpp: the cabinet's JVS I/O board (wheel, pedals, shifter,
   START, coins, test/service) as GP2X pad bits, plus absolute analog
   positions for the driver's analog ports (inptport.cpp). */
void naomi_input_init(void);
unsigned long naomi_pad(int njoy);
void naomi_input_poll(void);
void naomi_input_debug(char *out);

/* Live figures, read through the NetDIMM (naomi_dimmcomms.cpp,
   naomi-pi-tools' pi/live.py): 32-bit words, refreshed once a second. Times in
   tenths of a ms per emulated frame (upload: per frame shown). */
typedef struct {
	uint32_t magic;          /* +0x00 "MAME" (0x454d414d) */
	uint32_t version;        /* +0x04 1 */
	uint32_t seconds;        /* +0x08 */
	uint32_t emulated;       /* +0x0c frames emulated in the last second */
	uint32_t shown;          /* +0x10 frames shown */
	uint32_t busy;           /* +0x14 % of the time not waiting for the PVR */
	uint32_t game_loops;     /* +0x18 Super Sprint: main loop passes (60 = full speed) */
	uint32_t cpu0, cpu1, cpu2, sound, video, upload, sound_out; /* +0x1c..+0x38 */
	uint32_t dimm_requests;  /* +0x3c */
	uint32_t emulated_worst; /* +0x40 worst second (poke 0 to start over) */
	uint32_t pieces_mo, pieces_pf, pieces_al; /* +0x44 PVR renderer: quads of the last frame */
} naomi_live_t;
extern volatile naomi_live_t naomi_live;
void naomi_dimmcomms_start(void);

#endif
