/* The driver list of a NAOMI build: one game (Makefile.naomi GAME=...). */
#include "driver.h"

#define NAOMI_DRV2(g) g##_driver
#define NAOMI_DRV(g) NAOMI_DRV2(g)

extern struct GameDriver NAOMI_DRV(NAOMI_GAME);

const struct GameDriver *drivers[] =
{
	&NAOMI_DRV(NAOMI_GAME),
	0
};
