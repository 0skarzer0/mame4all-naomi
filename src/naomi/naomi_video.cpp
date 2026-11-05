/* NAOMI video: the game's 8-bit bitmap goes through the palette
   (gp2x_sdlwrapper_palette, RGB565) into a 512x256 PVR texture by the store
   queues (16-bit pixels, 32-byte bursts: the NAOMI's video RAM does not take
   byte writes), and one textured quad covers the 640x480 screen (31 kHz).
   Two textures, used in turn, so the one being drawn is never written. */

#include <kos.h>
#include <dc/sq.h>
#include <dc/pvr.h>

#include "driver.h"
#include "minimal.h"
#include "naomi.h"
#include <stdio.h>
#include <string.h>
#include "prof.h"

#ifndef NAOMI_FPS_DEFAULT
#define NAOMI_FPS_DEFAULT 0
#endif

extern struct osd_bitmap *scrbitmap;
extern int gp2x_xoffset, gp2x_yoffset, gp2x_width, gp2x_height;

#define TEX_W 512
#define TEX_H 512

static pvr_ptr_t tex[2];
static uint64_t wait_us = 0;   /* time spent waiting for the display */
static int cur = 0;
static int video_up = 0;

void naomi_video_init(void)
{
	if (video_up)
		return;
	pvr_init_params_t params = {
		{ PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
		64 * 1024, 0, 0, 0, 0, 0
	};
	pvr_init(&params);
	pvr_set_bg_color(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 2; i++) {
		tex[i] = pvr_mem_malloc(TEX_W * TEX_H * 2);
		sq_clr(tex[i], TEX_W * TEX_H * 2);
	}
	video_up = 1;
}

/* Frame rate overlay, composed in main RAM and sent with the picture by
   the store queues: nothing writes the texture directly (direct CPU stores
   into video RAM hung the real board after a while). */
#define OVL_ROWS 50
static uint16_t ovl[OVL_ROWS][TEX_W] __attribute__((aligned(32)));
static int ovl_len[OVL_ROWS];   /* overlay pixels at the start of each row */
static int ovl_on = 0;
static uint16_t row_buf[TEX_W] __attribute__((aligned(32)));

static void upload(uint16_t *dst, const unsigned char *src, int pitch, int width, int height)
{
	const short *pal = gp2x_sdlwrapper_palette;
	uint32_t *d = sq_lock(dst);
	const int skip = (TEX_W - width) >> 1;   /* in 32-bit words */
	for (int y = 0; y < height; y++) {
		const unsigned char *s = src + y * pitch;
		if (ovl_on && y < OVL_ROWS && ovl_len[y]) {
			int n = ovl_len[y] < width ? ovl_len[y] : width;
			for (int x = 0; x < width; x++)
				row_buf[x] = x < n ? ovl[y][x] : (uint16_t)pal[s[x]];
			const uint32_t *r = (const uint32_t *)row_buf;
			for (int x = 0; x < width; x += 16) {
				for (int k = 0; k < 8; k++)
					d[k] = r[k];
				sq_flush(d);
				d += 8;
				r += 8;
			}
			d += skip;
			continue;
		}
		for (int x = 0; x < width; x += 16) {
			__asm__("pref @%0" : : "r"(s + 32));
			for (int k = 0; k < 8; k++) {
				uint32_t o = (uint16_t)pal[s[0]] | ((uint32_t)(uint16_t)pal[s[1]] << 16);
				d[k] = o;
				s += 2;
			}
			sq_flush(d);
			d += 8;
		}
		d += skip;
	}
	sq_unlock();
}

static void draw(int width, int height)
{
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_vertex_t v;
	const float u1 = (float)width / TEX_W, v1 = (float)height / TEX_H;

	uint64_t w0 = timer_us_gettime64();
	pvr_wait_ready();
	wait_us += timer_us_gettime64() - w0;
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
	                 TEX_W, TEX_H, tex[cur], PVR_FILTER_BILINEAR);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	v.argb = 0xffffffff;
	v.oargb = 0;
	v.z = 1.0f;
	v.flags = PVR_CMD_VERTEX;
	v.x = 0.0f;   v.y = 480.0f; v.u = 0.0f; v.v = v1; pvr_prim(&v, sizeof(v));
	v.x = 0.0f;   v.y = 0.0f;   v.u = 0.0f; v.v = 0.0f; pvr_prim(&v, sizeof(v));
	v.x = 640.0f; v.y = 480.0f; v.u = u1;   v.v = v1; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = 640.0f; v.y = 0.0f;   v.u = u1;   v.v = 0.0f; pvr_prim(&v, sizeof(v));

	pvr_list_finish();
	pvr_scene_finish();
	cur ^= 1;
}

void naomi_present(const unsigned char *src, int pitch, int width, int height)
{
	width &= ~15;
	if (width > TEX_W) width = TEX_W;
	if (height > TEX_H) height = TEX_H;
	if (width <= 0 || height <= 0)
		return;
	ovl_on = 0;
	upload((uint16_t *)tex[cur], src, pitch, width, height);
	draw(width, height);
}

/* Frame rate overlay (top left): frames emulated / frames shown in the
   last second. Drawn into the texture after the upload, 16-bit stores. */
int naomi_show_fps = NAOMI_FPS_DEFAULT;
extern unsigned mame4all_totalframes;

static const unsigned char digits3x5[17][5] = {
	{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
	{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
	{1,1,2,4,4}, /* '/' */
	{2,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},{7,4,6,4,4} /* A-F */
};

static void overlay_text(int row, const char *txt)
{
	/* each font pixel 2x2 texels, white on black, rows row*12+2 .. +13 */
	int x0 = 2;
	for (const char *c = txt; *c && x0 + 8 <= TEX_W; c++) {
		int g = (*c == '/') ? 10 : (*c >= '0' && *c <= '9') ? *c - '0' :
		        (*c >= 'A' && *c <= 'F') ? *c - 'A' + 11 : -1;
		for (int y = 0; y < 12; y++) {
			int ty = row * 12 + y + 2;
			if (ty >= OVL_ROWS)
				break;
			for (int x = 0; x < 8; x++) {
				int on = 0;
				int fy = (y - 1) / 2, fx = (x - 1) / 2;
				if (g >= 0 && y >= 1 && y < 11 && x >= 1 && x < 7)
					on = (digits3x5[g][fy] >> (2 - fx)) & 1;
				ovl[ty][x0 + x] = on ? 0xffff : 0x0000;
			}
			if (ovl_len[ty] < x0 + 8)
				ovl_len[ty] = x0 + 8;
		}
		x0 += 8;
	}
}

static unsigned frames_shown = 0, last_total = 0;
static uint64_t stats_from = 0;
static char fps_text[16] = "";
static char prof_text[48] = "";

static void build_overlay(void)
{
	ovl_on = naomi_show_fps && fps_text[0];
	if (!ovl_on)
		return;
	memset(ovl_len, 0, sizeof(ovl_len));
	overlay_text(0, fps_text);
#ifdef PROFILER_MAME4ALL
	overlay_text(1, prof_text);
#else
	char dbg[48];
	naomi_input_debug(dbg);
	overlay_text(1, dbg);
#endif
#ifdef NAOMI_PCHIST
	extern char naomi_pch_text[3][64];
	for (int i = 0; i < 2; i++)
		if (naomi_pch_text[i][0])
			overlay_text(2 + i, naomi_pch_text[i]);
#endif
}

static void update_display_naomi(void)
{
	const int pitch = (int)(scrbitmap->line[1] - scrbitmap->line[0]);
	int width = gp2x_width & ~15, height = gp2x_height;
	if (width > TEX_W) width = TEX_W;
	if (height > TEX_H) height = TEX_H;

	frames_shown++;
	uint64_t now = timer_ms_gettime64();
	if (!stats_from)
		stats_from = now;
	else if (now - stats_from >= 1000) {
		unsigned emu = mame4all_totalframes - last_total;
#ifdef PROFILER_MAME4ALL
		/* tenths of a ms per emulated frame: all, 68K main, Z80, 68K sub,
		   sound, game video, our upload+draw (per frame shown), sound out */
		{
			static const int slot[8] = { 0, 2, 3, 4, 6, 7, 8, 9 };
			char *p = prof_text;
			for (int i = 0; i < 8; i++) {
				unsigned div = (slot[i] == 8) ? frames_shown : emu;
				unsigned v = div ? (unsigned)(mame4all_prof_sum[slot[i]] / (div * 100)) : 0;
				p += sprintf(p, "%u ", v);
			}
			for (int i = 0; i < MAME4ALL_PROFILER_MAX; i++)
				mame4all_prof_sum[i] = 0;
		}
#endif
		unsigned busy = 100 - (unsigned)(wait_us / ((now - stats_from) * 10));
		sprintf(fps_text, "%u/%u %u", emu, frames_shown, busy);
		wait_us = 0;
		last_total = mame4all_totalframes;
		frames_shown = 0;
		stats_from = now;
	}
	build_overlay();

	mame4all_prof_start(8);
	upload((uint16_t *)tex[cur], scrbitmap->line[gp2x_yoffset] + gp2x_xoffset, pitch, width, height);
	mame4all_prof_end(8);
	draw(width, height);
}

update_display_func p_update_display = update_display_naomi;

void update_p_update_display_func(unsigned n)
{
	(void)n;
	p_update_display = update_display_naomi;
}

void fade_me(void) { }
