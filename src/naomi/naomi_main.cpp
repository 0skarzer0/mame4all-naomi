/* NAOMI: start the one game of this build straight away (no menu). What
   gp2x.cpp did, without the game list and the options screen. The ROM zip
   is in the romdisk (/rd/roms/GAME.zip). */

#include <kos.h>
#include <stdio.h>
#include <string.h>

#include "driver.h"
#include "minimal.h"
#include "naomi.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

int msdos_init_sound(void);
void msdos_shutdown_sound(void);
void msdos_init_input(void);
void msdos_shutdown_input(void);

extern unsigned int soundcard;
extern int gp2x_frameskip;

struct GameOptions options;
#define NAOMI_STR2(x) #x
#define NAOMI_STR(x) NAOMI_STR2(x)
char playgame[16] = NAOMI_STR(NAOMI_GAME);

int osd_init(void)
{
	if (msdos_init_sound())
		return 1;
	msdos_init_input();
	return 0;
}

void osd_exit(void)
{
	msdos_shutdown_sound();
	msdos_shutdown_input();
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	int game_index = -1;

#ifdef NAOMI_BOOTLOG
	thd_sleep(6000);
#endif
	gp2x_video_init();
	gp2x_clear_screen();
	gp2x_gamelist_zero();

	for (int i = 0; drivers[i]; i++)
		if (strcasecmp(playgame, drivers[i]->name) == 0) {
			game_index = i;
			break;
		}
	if (game_index < 0) {
		gp2x_text_log((char *)"Game not in this build");
		for (;;) thd_sleep(1000);
	}

	memset(&options, 0, sizeof(options));
	options.frameskip = -1;          /* automatic (video.cpp, NAOMI) */
	options.samplerate = 22050;
	options.samplebits = 16;
#ifndef NAOMI_SOUND
#define NAOMI_SOUND 1
#endif
	soundcard = NAOMI_SOUND;
#ifdef NAOMI_RATE
	{
		extern int default_sample_rate;
		default_sample_rate = NAOMI_RATE;
	}
#endif

	char text[64];
	sprintf(text, "Loading \"%s\"...", drivers[game_index]->description);
	gp2x_text_log(text);

	int res = run_game(game_index, &options);

	gp2x_text_log((char *)(res ? "LOAD FAILED" : "Game stopped"));
	for (;;) thd_sleep(1000);
	return 0;
}
