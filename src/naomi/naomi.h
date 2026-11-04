/* NAOMI platform layer for MAME4ALL (KallistiOS, KOS_SUBARCH=naomi). */
#ifndef NAOMI_PLATFORM_H
#define NAOMI_PLATFORM_H

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

#endif
