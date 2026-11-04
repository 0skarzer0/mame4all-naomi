/* NAOMI: what minimal.cpp (the SDL wrapper) did on the Dreamcast build, on
   KallistiOS directly: the 8-bit text screen, the GP2X-style pad word and
   memory helpers. Video is in naomi_video.cpp, sound in naomi_audio.cpp. */

#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "minimal.h"
#include "naomi.h"

int gp2x_sdlwrapper_bpp = 8;
short gp2x_sdlwrapper_palette[256];
int gp2x_sdlwrapper_is_single = 0;
void *gp2x_sdlwrapper_screen_pixels = NULL;
SDL_Surface *gp2x_sdlwrapper_screen = NULL;
float gp2x_sdlwrapper_ticksdivisor = 1.0;

/* What gp2x_menu.cpp provided (there is no menu here). */
int gp2x_sound_enable = 1;
char gp2x_path_mame[24] = "/rd/";
/* CPU clocks in percent of the real ones (timer.cpp): main CPUs, sound CPU. */
#ifndef NAOMI_CLOCK_CPU
#define NAOMI_CLOCK_CPU 100
#endif
#ifndef NAOMI_CLOCK_SOUND
#define NAOMI_CLOCK_SOUND 100
#endif
int gp2x_clock_cpu = NAOMI_CLOCK_CPU;
int gp2x_clock_sound = NAOMI_CLOCK_SOUND;

static unsigned char text_screen[320 * 240] __attribute__((aligned(32)));
static SDL_PixelFormat dummy_format;
static SDL_Surface dummy_surface;

void gp2x_init(int tickspersecond, int bpp, int rate, int bits, int stereo, int hz)
{
	(void)tickspersecond; (void)bpp; (void)rate; (void)bits; (void)stereo; (void)hz;
	dummy_surface.format = &dummy_format;
	dummy_surface.w = 320;
	dummy_surface.h = 240;
	dummy_surface.pitch = 320;
	dummy_surface.pixels = text_screen;
	gp2x_sdlwrapper_screen = &dummy_surface;
	gp2x_sdlwrapper_screen_pixels = text_screen;
	memset(text_screen, 0, sizeof(text_screen));
	naomi_video_init();
	naomi_input_init();
}

void gp2x_deinit(void)
{
	naomi_audio_quit();
	arch_reboot();
}

/* The text screen (log messages before the game starts). */
void gp2x_video_flip(void)
{
	gp2x_sdlwrapper_is_single = 0;
	naomi_present(text_screen, 320, 320, 240);
}

void gp2x_video_flip_single(void)
{
	gp2x_video_flip();
	gp2x_sdlwrapper_is_single = 1;
}

unsigned long gp2x_joystick_read_n(int njoy)
{
	return naomi_pad(njoy);
}

void SetGP2XClock(int mhz) { (void)mhz; }

/* Profiler hooks (prof.h) are compiled out; these keep the linker happy if
   a file still references them. */
void dc_change_to_normal(void) { }
void dc_change_to_hw_scaled(void) { }

#ifdef PROFILER_MAME4ALL
/* prof.h: time per section, in microseconds (shown by naomi_video). */
unsigned mame4all_prof_total = 8;
unsigned long long mame4all_prof_initial[MAME4ALL_PROFILER_MAX];
unsigned long long mame4all_prof_sum[MAME4ALL_PROFILER_MAX];
unsigned long long mame4all_prof_executed[MAME4ALL_PROFILER_MAX];
int mame4all_prof_started[MAME4ALL_PROFILER_MAX];
void mame4all_prof_init(void) { }
void mame4all_prof_add(char *msg) { (void)msg; }
void mame4all_prof_show(void) { }
void mame4all_prof_setmsg(int n, char *msg) { (void)n; (void)msg; }
#endif
