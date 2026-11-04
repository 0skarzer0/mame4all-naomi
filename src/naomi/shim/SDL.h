/* Minimal SDL 1.2 stand-in for the NAOMI build: only the types and the few
   calls the MAME core and the kept OSD files use. */
#ifndef NAOMI_SDL_SHIM_H
#define NAOMI_SDL_SHIM_H
#include <stdint.h>
#include <arch/timer.h>
#include <kos/thread.h>

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

typedef struct SDL_Color { Uint8 r, g, b, unused; } SDL_Color;
typedef struct SDL_PixelFormat { int dummy; } SDL_PixelFormat;
typedef struct SDL_Surface { SDL_PixelFormat *format; int w, h, pitch; void *pixels; } SDL_Surface;

static inline Uint32 SDL_GetTicks(void) { return (Uint32)timer_ms_gettime64(); }
static inline void SDL_Delay(Uint32 ms) { thd_sleep(ms); }
static inline Uint32 SDL_MapRGB(SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b)
{ (void)f; return ((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | (b >> 3); }
static inline Uint32 SDL_MapRGBA(SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{ (void)a; return SDL_MapRGB(f, r, g, b); }
static inline int SDL_Flip(SDL_Surface *s) { (void)s; return 0; }
#endif
