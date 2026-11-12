/* NAOMI video. The game's 8-bit bitmap goes as it is into a 512x512
   8-bit paletted PVR texture (twiddled, as the PVR wants paletted
   textures), sent by the store queues in 32-byte blocks (the NAOMI's video
   RAM does not take byte writes); the game's palette (RGB565) goes to the
   PVR palette RAM. One quad, unfiltered (NAOMI_FILTER=1 for bilinear) and
   unscaled: 1:1 pixels centred on the 640x480 screen (31 kHz), or whole
   pixels doubled when that still fits (320x224 -> 640x448). Two textures, used in turn, so the one
   being drawn is never written. The frame rate overlay is a separate small
   ARGB1555 texture drawn over the picture.
   -DNAOMI_VIDEO16 keeps the earlier path (palette applied by the CPU into an
   RGB565 nontwiddled texture). */

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
#ifdef NAOMI_VIDEO16
#define TEX_BPP 2
#else
#define TEX_BPP 1
#endif
#define OVL_TEX_H 64
#ifndef NAOMI_FILTER
#define NAOMI_FILTER 0
#endif

static pvr_ptr_t ovl_tex;
static uint32_t tw_x[TEX_W], tw_y[TEX_H];   /* twiddle: bits of x / y spread out */
static int blk_dx[32], blk_dy[32];        /* texel order in a 32-byte block (4x8) */

static pvr_ptr_t tex[2];
static uint64_t wait_us = 0;   /* time spent waiting for the display */
static int cur = 0;
static int video_up = 0;

void naomi_video_init(void)
{
	if (video_up)
		return;
	pvr_init_params_t params = {
		{ PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
		64 * 1024, 0, 0, 0, 0, 0
	};
	pvr_init(&params);
	pvr_set_bg_color(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 2; i++) {
		tex[i] = pvr_mem_malloc(TEX_W * TEX_H * TEX_BPP);
		sq_clr(tex[i], TEX_W * TEX_H * TEX_BPP);
	}
	ovl_tex = pvr_mem_malloc(TEX_W * OVL_TEX_H * 2);
	sq_clr(ovl_tex, TEX_W * OVL_TEX_H * 2);
#ifndef NAOMI_VIDEO16
	pvr_set_pal_format(PVR_PAL_RGB565);
	for (int v = 0; v < TEX_W; v++) {
		unsigned sp = 0;
		for (int b = 0; b < 9; b++)
			sp |= ((v >> b) & 1) << (2 * b);
		tw_y[v] = sp;
		tw_x[v] = sp << 1;
	}
	for (int i = 0; i < 32; i++) {
		blk_dx[i] = ((i >> 1) & 1) | ((i >> 2) & 2);
		blk_dy[i] = (i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4);
	}
#endif
	video_up = 1;
}

/* Frame rate overlay: composed in main RAM (ARGB1555: 0 = see-through) and
   sent by the store queues to its own texture when it changes; nothing
   writes video RAM directly (direct CPU stores into video RAM hung the real
   board after a while). */
#define OVL_ROWS OVL_TEX_H
static uint16_t ovl[OVL_ROWS][TEX_W] __attribute__((aligned(32)));
static int ovl_len[OVL_ROWS];
static uint16_t row_buf[TEX_W] __attribute__((aligned(32)));

static void upload16(uint16_t *dst, const unsigned char *src, int pitch, int width, int height)
{
	const short *pal = gp2x_sdlwrapper_palette;
	uint32_t *d = sq_lock(dst);
	const int skip = (TEX_W - width) >> 1;   /* in 32-bit words */
	for (int y = 0; y < height; y++) {
		const unsigned char *s = src + y * pitch;
		if (0) {
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

#ifndef NAOMI_VIDEO16
/* 8-bit paletted twiddled upload. A 32-byte store queue block holds 32
   consecutive twiddled texels: 4 columns x 8 rows. Rows past the picture
   repeat its last row. */
static void upload8(uint8_t *dst, const unsigned char *src, int pitch, int width, int height)
{
	uint8_t *sqb = (uint8_t *)sq_lock(dst);
	int off[32];
	for (int y0 = 0; y0 < height; y0 += 8) {
		const unsigned char *rows[8];
		for (int r = 0; r < 8; r++)
			rows[r] = src + (y0 + r < height ? y0 + r : height - 1) * pitch;
		for (int i = 0; i < 32; i++)
			off[i] = (int)(rows[blk_dy[i]] - src) + blk_dx[i];
		const uint32_t ty = tw_y[y0];
		for (int x0 = 0; x0 < width; x0 += 4) {
			const unsigned char *s = src + x0;
			uint32_t *d = (uint32_t *)(sqb + (ty | tw_x[x0]));
			for (int k = 0; k < 8; k++) {
				const int *o = off + 4 * k;
				d[k] = s[o[0]] | (s[o[1]] << 8) | (s[o[2]] << 16) | ((uint32_t)s[o[3]] << 24);
			}
			sq_flush(d);
		}
	}
	sq_unlock();
}

static void upload_palette(void)
{
	const short *pal = gp2x_sdlwrapper_palette;
	for (int i = 0; i < 256; i++)
		pvr_set_pal_entry(i, (uint16_t)pal[i]);
}
#endif

static void upload(void *dst, const unsigned char *src, int pitch, int width, int height)
{
#ifdef NAOMI_VIDEO16
	upload16((uint16_t *)dst, src, pitch, width, height);
#else
	upload8((uint8_t *)dst, src, pitch, width, height);
#endif
}

static void quad(float x0, float y0, float x1, float y1, float u1, float v1)
{
	pvr_vertex_t v;
	v.argb = 0xffffffff;
	v.oargb = 0;
	v.z = 1.0f;
	v.flags = PVR_CMD_VERTEX;
	v.x = x0; v.y = y1; v.u = 0.0f; v.v = v1;   pvr_prim(&v, sizeof(v));
	v.x = x0; v.y = y0; v.u = 0.0f; v.v = 0.0f; pvr_prim(&v, sizeof(v));
	v.x = x1; v.y = y1; v.u = u1;   v.v = v1;   pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x1; v.y = y0; v.u = u1;   v.v = 0.0f; pvr_prim(&v, sizeof(v));
}

static int ovl_on = 0;

static void draw(int width, int height)
{
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	const float u1 = (float)width / TEX_W, v1 = (float)height / TEX_H;

	uint64_t w0 = timer_us_gettime64();
	pvr_wait_ready();
	wait_us += timer_us_gettime64() - w0;
#ifndef NAOMI_VIDEO16
	upload_palette();
#endif
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);
#ifdef NAOMI_VIDEO16
	pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
	                 TEX_W, TEX_H, tex[cur], NAOMI_FILTER ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
#else
	pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0),
	                 TEX_W, TEX_H, tex[cur], NAOMI_FILTER ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
#endif
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	{
		/* no scaling: 1:1 pixels, centred; doubled (whole pixels) only when
		   the doubled picture still fits the 640x480 screen */
		int f = (width * 2 <= 640 && height * 2 <= 480) ? 2 : 1;
		int w = width * f, h = height * f;
		float x0 = (float)((640 - w) / 2), y0 = (float)((480 - h) / 2);
		quad(x0, y0, x0 + w, y0 + h, u1, v1);
	}
	pvr_list_finish();

	pvr_list_begin(PVR_LIST_TR_POLY);
	if (ovl_on) {
		/* overlay texels are 1:1 on screen horizontally: 512 texels -> 512 px */
		pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
		                 TEX_W, OVL_TEX_H, ovl_tex, PVR_FILTER_NONE);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));
		quad(0.0f, 0.0f, 512.0f, (float)OVL_TEX_H, 1.0f, 1.0f);
	}
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
	upload(tex[cur], src, pitch, width, height);
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
				ovl[ty][x0 + x] = on ? 0xffff : 0x8000;
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

static void send_overlay(void)
{
	uint32_t *d = (uint32_t *)sq_lock(ovl_tex);
	const uint32_t *s = (const uint32_t *)ovl;
	for (int i = 0; i < (int)(sizeof(ovl) / 32); i++) {
		for (int k = 0; k < 8; k++)
			d[k] = s[k];
		sq_flush(d);
		d += 8;
		s += 8;
	}
	sq_unlock();
}

static void build_overlay(int changed)
{
	ovl_on = naomi_show_fps && fps_text[0];
	if (!ovl_on || !changed)
		return;
	memset(ovl, 0, sizeof(ovl));
	memset(ovl_len, 0, sizeof(ovl_len));
	overlay_text(0, fps_text);
#ifdef PROFILER_MAME4ALL
	overlay_text(1, prof_text);
#else
	char dbg[48];
	naomi_input_debug(dbg);
	overlay_text(1, dbg);
#endif
#if defined(NAOMI_PCHIST) || defined(NAOMI_T11STAT) || defined(NAOMI_T11RC_MISSES) || defined(NAOMI_RAMCRC)
	extern char naomi_pch_text[3][64];
	for (int i = 0; i < 3; i++)
		if (naomi_pch_text[i][0])
			overlay_text(2 + i, naomi_pch_text[i]);
#endif
	send_overlay();
}

#ifdef NAOMI_RAMCRC
/* Debug: CRC of the main CPU's RAM at fixed emulated frames, to check that
   two builds (interpreter / recompiled) run the game identically. */
void naomi_ram_crc(void)
{
	extern char naomi_pch_text[3][64];
	static const unsigned at[3] = { 600, 1200, 1800 };
	static unsigned crc[3];
	for (int i = 0; i < 3; i++)
		if (mame4all_totalframes == at[i] && !crc[i])
			{
			unsigned h = 2166136261u;
			const unsigned char *m = Machine->memory_region[0];
			for (int k = 0; k < 0x1000; k++)
				h = (h ^ m[k]) * 16777619u;
			crc[i] = h | 1;
		}
	sprintf(naomi_pch_text[2], "%X %X %X", crc[0], crc[1], crc[2]);
}
#endif

static void update_display_naomi(void)
{

	const int pitch = (int)(scrbitmap->line[1] - scrbitmap->line[0]);
	int width = gp2x_width & ~15, height = gp2x_height;
	if (width > TEX_W) width = TEX_W;
	if (height > TEX_H) height = TEX_H;

	frames_shown++;
	int stats_changed = 0;
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
#ifdef NAOMI_T11RC_MISSES
		{
			extern unsigned t11_rc_hits, t11_rc_misses, t11_rc_ins;
			extern char naomi_pch_text[3][64];
			extern void t11_rc_report (char *);
			/* per emulated frame: blocks run, interpreted instructions; then top missed addresses */
			sprintf(naomi_pch_text[0], "%u %u %u", emu ? t11_rc_hits / emu : 0, emu ? t11_rc_misses / emu : 0,
			        emu ? t11_rc_ins / emu : 0);
			t11_rc_report(naomi_pch_text[1]);
			t11_rc_hits = t11_rc_misses = t11_rc_ins = 0;
		}
#endif
#ifdef NAOMI_T11STAT
		{
			extern unsigned t11_stat_ins, t11_stat_slow;
			extern unsigned long long t11_stat_slow_us;
			extern char naomi_pch_text[3][64];
			/* per emulated frame: T11 instructions, slow accesses, tenths of ms in their handlers */
			sprintf(naomi_pch_text[0], "%u %u %u", emu ? t11_stat_ins / emu : 0, emu ? t11_stat_slow / emu : 0,
			        emu ? (unsigned)(t11_stat_slow_us / (emu * 100)) : 0);
			t11_stat_ins = t11_stat_slow = 0; t11_stat_slow_us = 0;
		}
#endif
#ifdef NAOMI_SSPRINT1
		{
			extern unsigned naomi_ss_loops;
			sprintf(fps_text, "%u/%u %u / %u", emu, frames_shown, busy, naomi_ss_loops);
			naomi_ss_loops = 0;
		}
#else
		sprintf(fps_text, "%u/%u %u", emu, frames_shown, busy);
#endif
		wait_us = 0;
		stats_changed = 1;
		last_total = mame4all_totalframes;
		frames_shown = 0;
		stats_from = now;
	}
	{
		static int was_on = 0;
		build_overlay(stats_changed || !was_on);
		was_on = ovl_on;
	}

	mame4all_prof_start(8);
	upload(tex[cur], scrbitmap->line[gp2x_yoffset] + gp2x_xoffset, pitch, width, height);
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
