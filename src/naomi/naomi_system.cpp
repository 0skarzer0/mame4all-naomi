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

#ifdef NAOMI_PCHIST
/* Debug: where each CPU is when its time slice ends (top 3 PCs, % of its
   slices) and the % of the asked cycles it really ran (spin hacks end a
   slice early). Refreshed every 600 slices of CPU 0, shown by naomi_video. */
#include <stdio.h>
#define PCH 64
char naomi_pch_text[3][64];
static unsigned pch_pc[3][PCH], pch_n[3][PCH];
static unsigned long long pch_asked[3], pch_ran[3];
static unsigned pch_slices[3];
void naomi_pchist(int cpu, unsigned pc, int asked, int ran)
{
	if (cpu > 2) return;
	pch_asked[cpu] += asked; pch_ran[cpu] += ran; pch_slices[cpu]++;
	for (int i = 0; i < PCH; i++) {
		if (pch_n[cpu][i] && pch_pc[cpu][i] == pc) { pch_n[cpu][i]++; break; }
		if (!pch_n[cpu][i]) { pch_pc[cpu][i] = pc; pch_n[cpu][i] = 1; break; }
	}
	if (cpu == 0 && pch_slices[0] % 600 == 0) {
		for (int c = 0; c < 3; c++) {
			char *p = naomi_pch_text[c];
			p[0] = 0;
			if (!pch_slices[c]) continue;
			for (int k = 0; k < 3; k++) {
				int b = -1; unsigned bn = 0;
				for (int j = 0; j < PCH; j++) if (pch_n[c][j] > bn) { bn = pch_n[c][j]; b = j; }
				if (b < 0) break;
				p += sprintf(p, "%X %u ", pch_pc[c][b] & 0xffffff, bn * 100 / pch_slices[c]);
				pch_n[c][b] = 0;
			}
			sprintf(p, "/ %u", pch_asked[c] ? (unsigned)(pch_ran[c] * 100 / pch_asked[c]) : 0);
			for (int j = 0; j < PCH; j++) pch_n[c][j] = 0;
			pch_asked[c] = pch_ran[c] = 0; pch_slices[c] = 0;
		}
	}
}
#endif
