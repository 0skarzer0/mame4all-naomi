/* Atari System 2 (Super Sprint) drawn by the NAOMI's PVR instead of the CPU.

   MAME draws the playfield, the motion objects and the alphanumerics into an
   8-bit bitmap that is then sent to the PVR every frame (~6 ms + ~6.5 ms on
   the board for 512x384). Here the PVR draws the picture itself:

   - all the game's graphics (ROM) are decoded once into 4-bit paletted
     twiddled textures in video RAM: playfield tiles (16384 8x8), motion
     objects (2048 16x16), alphanumerics (1024 8x8, 2 bits widened to 4);
   - the playfield (128x64 tiles) is one 1024x512 8-bit paletted texture in
     final pens; only the tiles MAME marks dirty are rewritten (64 bytes
     each), and one quad with wrapping texture coordinates scrolls it;
   - the motion objects are one quad per 16x16 piece, in display-list order
     (later ones on top), clipped to their display-list band;
   - the playfield's high-priority pens (8-15) over the motion objects use
     MAME's rule from the schematics, ((mo_pri*2) + pf_pri) & 4, tile by tile
     inside each motion object's rectangle, as atarisys2_vh_screenrefresh;
   - the alphanumerics are one quad per non-empty character.
   Colours: the PVR palette RAM (ARGB1555) holds the 256 screen pens in bank
   0 (8-bit textures) and, from entry 256, one 16-entry bank per colour code of
   each layer, built from the gfx colour tables every frame, with the
   transparent pens' alpha bit clear (punch-through list).
   Full screen x1.25 with bilinear filter by default (NAOMI_SS_FULL 0: 1:1, no filter). */
#include <kos.h>
#include <dc/pvr.h>
#include <dc/sq.h>
#include <string.h>
#include "driver.h"
#include "naomi.h"

#define SCR_W 640
#define SCR_H 480
#define VIS_W 512
#define VIS_H 384
/* Where the picture goes. With NAOMI_SS_FULL 2 (too slow on the board: 25-30 fps) it fills the 640x480
   screen with a "sharp bilinear" look: naomi_video.cpp has it drawn here at
   x2, no filter, into a 1024x768 texture (four 512x384 renders), then shows
   that texture on the whole screen with the bilinear filter.
   NAOMI_SS_FULL 3 (default): the whole picture drawn 1:1 without filter into a
   512x384 texture, and that texture shown on the whole screen with the
   bilinear filter (filtering each piece, as 1 does, loses the thin strokes of
   the punch-through text and shows the joins between tiles);
   NAOMI_SS_FULL 1: drawn straight at x1.25 with the bilinear filter;
   0: 1:1 pixels centred, no filter. */
#ifndef NAOMI_SS_FULL
#define NAOMI_SS_FULL 3
#endif
static float SC = NAOMI_SS_FULL == 1 ? (float)SCR_W / VIS_W : 1.0f;
static float OX = NAOMI_SS_FULL == 1 ? 0.0f : (float)((SCR_W - VIS_W) / 2);
static float OY = NAOMI_SS_FULL == 1 ? 0.0f : (float)((SCR_H - VIS_H) / 2);
static int FILTER = NAOMI_SS_FULL == 1 ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE;
static float INSET = NAOMI_SS_FULL == 1 ? 0.5f : 0.0f;   /* keep the filter inside each atlas element */

/* draw at scale sc, shifted by (ox, oy), with or without the bilinear filter */
void naomi_pvr_atarisys2_view(float sc, float ox, float oy, int filter)
{
	SC = sc;
	OX = ox;
	OY = oy;
	FILTER = filter ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE;
	INSET = filter ? 0.5f : 0.0f;
}

int naomi_pvr_atarisys2_sharp(void)
{
	return NAOMI_SS_FULL >= 2 ? NAOMI_SS_FULL : 0;
}

/* PVR palette banks (16 entries each) */
#define BANK_PFHI 16   /* 8 colours: playfield, pens 0-7 transparent */
#define BANK_MO   24   /* 4 colours: motion objects, pen 15 transparent */
#define BANK_AL   28   /* 8 colours: alphanumerics, pen 0 transparent */

static pvr_ptr_t pf_atlas, mo_atlas, al_atlas, pf_tex;
static int ready;

/* twiddle helpers: bit k of v to bit 2k (y) or 2k+1 (x) */
static uint32_t spread(uint32_t v)
{
	uint32_t r = 0;
	for (int b = 0; b < 16; b++)
		r |= ((v >> b) & 1) << (2 * b);
	return r;
}
static uint32_t compact(uint32_t v)
{
	uint32_t r = 0;
	for (int b = 0; b < 16; b++)
		r |= ((v >> (2 * b)) & 1) << b;
	return r;
}
static uint8_t blk_dx[256], blk_dy[256];   /* texel k of a twiddled block: x, y */

/* 4-bit atlas of N square elements of size S from a MAME gfx element set:
   element i is twiddled block i (its texels contiguous), so block i sits at
   x = compact(i >> 1) * S, y = compact(i) * S inside each square of side
   SQ (rectangular textures are squares side by side). */
static pvr_ptr_t make_atlas(const struct GfxElement *gfx, int s, int w, int h, int pen_or)
{
	const int n = gfx->total_elements;
	const int texels = s * s;
	const size_t bytes = (size_t)w * h / 2;
	uint8_t *buf = (uint8_t *)memalign(32, bytes);
	memset(buf, 0, bytes);
	for (int i = 0; i < n; i++) {
		uint8_t *d = buf + (size_t)i * texels / 2;
		for (int k = 0; k < texels; k += 2) {
			int x0 = blk_dx[k], y0 = blk_dy[k], x1 = blk_dx[k + 1], y1 = blk_dy[k + 1];
			if (s == 16) {   /* 16x16: texel k of 256 */
				x0 = compact(k >> 1); y0 = compact(k);
				x1 = compact((k + 1) >> 1); y1 = compact(k + 1);
			}
			int p0 = gfx->gfxdata->line[i * gfx->height + y0][x0] | pen_or;
			int p1 = gfx->gfxdata->line[i * gfx->height + y1][x1] | pen_or;
			d[k / 2] = (p0 & 15) | ((p1 & 15) << 4);
		}
	}
	pvr_ptr_t t = pvr_mem_malloc(bytes);
	pvr_txr_load(buf, t, bytes);
	free(buf);
	return t;
}

/* where element i of an atlas of side-SQ squares is, in texels */
static void atlas_pos(int i, int s, int sq, int *x, int *y)
{
	const int per_sq = (sq / s) * (sq / s);
	const int square = i / per_sq, b = i % per_sq;
	*x = square * sq + (int)compact((uint32_t)b >> 1) * s;
	*y = (int)compact((uint32_t)b) * s;
}

void naomi_pvr_atarisys2_init(void)
{
	for (int k = 0; k < 256; k++) {
		blk_dx[k] = compact(k >> 1) & 7;
		blk_dy[k] = compact(k) & 7;
	}
	pf_atlas = make_atlas(Machine->gfx[1], 8, 1024, 1024, 0);   /* 16384 tiles */
	mo_atlas = make_atlas(Machine->gfx[2], 16, 1024, 512, 0);   /* 2048 objects */
	al_atlas = make_atlas(Machine->gfx[0], 8, 256, 256, 0);     /* 1024 chars */
	pf_tex = pvr_mem_malloc(1024 * 512);
	sq_clr(pf_tex, 1024 * 512);
	ready = 1;
}

/* ---- per frame, filled by atarisys2_vh_screenrefresh ---- */

struct mo_piece { int16_t x, y, cy0, cy1; uint16_t pict; uint8_t color, hflip; };
struct pf_piece { int16_t x, y, cx0, cx1, cy0, cy1; uint16_t tile; uint8_t color; };
struct al_piece { int16_t x, y; uint16_t pict; uint8_t color; };

#define MAX_MO 1024
#define MAX_PF 2048
#define MAX_AL (64 * 48)
static struct mo_piece mos[MAX_MO];
static struct pf_piece pfs[MAX_PF];
static struct al_piece als[MAX_AL];
static int n_mo, n_pf, n_al;
static int scroll_x, scroll_y;

void naomi_pvr_atarisys2_begin(int xscroll, int yscroll)
{
	n_mo = n_pf = n_al = 0;
	scroll_x = xscroll;
	scroll_y = yscroll;
}

/* one 8x8 playfield tile into the 1024x512 8-bit texture, in final pens */
void naomi_pvr_atarisys2_pf_tile(int sx, int sy, int tile, int color)
{
	const struct GfxElement *gfx = Machine->gfx[1];
	const unsigned short *ct = gfx->colortable + color * 16;
	const int x = sx * 8, y = sy * 8;
	const uint32_t off = (uint32_t)(x / 512) * 512 * 512 + (spread(y / 8) | (spread((x % 512) / 8) << 1)) * 64;
	uint32_t w[16];
	uint8_t *b = (uint8_t *)w;
	for (int k = 0; k < 64; k++)
		b[k] = (uint8_t)ct[gfx->gfxdata->line[tile * 8 + blk_dy[k]][blk_dx[k]]];
	volatile uint32_t *d = (volatile uint32_t *)((uint8_t *)pf_tex + off);
	for (int i = 0; i < 16; i++)
		d[i] = w[i];
}

void naomi_pvr_atarisys2_mo(int x, int y, int pict, int color, int hflip, int clip_y0, int clip_y1)
{
	const struct GfxElement *gfx = Machine->gfx[2];
	if (gfx->pen_usage && !(gfx->pen_usage[pict] & ~0x8000))
		return;   /* only pen 15 (transparent) */
	if (n_mo < MAX_MO) {
		struct mo_piece *m = &mos[n_mo++];
		m->x = x; m->y = y; m->pict = pict; m->color = color; m->hflip = hflip ? 1 : 0;
		m->cy0 = clip_y0; m->cy1 = clip_y1;
	}
}

void naomi_pvr_atarisys2_pf_high(int x, int y, int tile, int color, int cx0, int cx1, int cy0, int cy1)
{
	const struct GfxElement *gfx = Machine->gfx[1];
	if (gfx->pen_usage && !(gfx->pen_usage[tile] & 0xff00))
		return;   /* no pen 8-15: nothing shows over the motion objects */
	if (n_pf < MAX_PF) {
		struct pf_piece *p = &pfs[n_pf++];
		p->x = x; p->y = y; p->tile = tile; p->color = color;
		p->cx0 = cx0; p->cx1 = cx1; p->cy0 = cy0; p->cy1 = cy1;
	}
}

void naomi_pvr_atarisys2_alpha(int x, int y, int pict, int color)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	if (gfx->pen_usage && !(gfx->pen_usage[pict] & ~1))
		return;   /* only the transparent pen: nothing to draw */
	if (n_al < MAX_AL) {
		struct al_piece *a = &als[n_al++];
		a->x = x; a->y = y; a->pict = pict; a->color = color;
	}
}

/* ---- drawing, called from naomi_video.cpp's scene ---- */

static uint16_t argb(uint16_t rgb565, int opaque)
{
	return (opaque ? 0x8000 : 0) | ((rgb565 >> 1) & 0x7fe0) | (rgb565 & 0x1f);
}

void naomi_pvr_atarisys2_palette(void)
{
	const short *pal = gp2x_sdlwrapper_palette;
	int i, c;
	for (i = 0; i < 256; i++)
		pvr_set_pal_entry(i, argb((uint16_t)pal[i], 1));
	const unsigned short *ct;
	ct = Machine->gfx[1]->colortable;
	for (c = 0; c < 8; c++)
		for (i = 0; i < 16; i++)
			pvr_set_pal_entry((BANK_PFHI + c) * 16 + i, argb((uint16_t)pal[ct[c * 16 + i]], i >= 8));
	ct = Machine->gfx[2]->colortable;
	for (c = 0; c < 4; c++)
		for (i = 0; i < 16; i++)
			pvr_set_pal_entry((BANK_MO + c) * 16 + i, argb((uint16_t)pal[ct[c * 16 + i]], i != 15));
	ct = Machine->gfx[0]->colortable;
	for (c = 0; c < 8; c++)
		for (i = 0; i < 16; i++)
			pvr_set_pal_entry((BANK_AL + c) * 16 + i, i < 4 ? argb((uint16_t)pal[ct[c * 4 + i]], i != 0) : 0);
}

static void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float z)
{
	pvr_vertex_t v;
	v.argb = 0xffffffff;
	v.oargb = 0;
	v.z = z;
	v.flags = PVR_CMD_VERTEX;
	x0 = OX + x0 * SC; x1 = OX + x1 * SC;
	y0 = OY + y0 * SC; y1 = OY + y1 * SC;
	v.x = x0; v.y = y1; v.u = u0; v.v = v1; pvr_prim(&v, sizeof(v));
	v.x = x0; v.y = y0; v.u = u0; v.v = v0; pvr_prim(&v, sizeof(v));
	v.x = x1; v.y = y1; v.u = u1; v.v = v1; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x1; v.y = y0; v.u = u1; v.v = v0; pvr_prim(&v, sizeof(v));
}

static void header(int list, int fmt, int w, int h, pvr_ptr_t t)
{
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_txr(&cxt, list, fmt, w, h, t, FILTER);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
}

/* a rectangle of an element, clipped to [cx0,cx1]x[cy0,cy1] (bitmap pixels, inclusive) */
static void piece(int x, int y, int s, int ex, int ey, int tw, int th, int hflip,
                  int cx0, int cx1, int cy0, int cy1, float z)
{
	int x0 = x, y0 = y, x1 = x + s, y1 = y + s;
	if (cx0 < 0) cx0 = 0;
	if (cy0 < 0) cy0 = 0;
	if (cx1 > VIS_W - 1) cx1 = VIS_W - 1;
	if (cy1 > VIS_H - 1) cy1 = VIS_H - 1;
	if (x0 < cx0) x0 = cx0;
	if (y0 < cy0) y0 = cy0;
	if (x1 > cx1 + 1) x1 = cx1 + 1;
	if (y1 > cy1 + 1) y1 = cy1 + 1;
	if (x0 >= x1 || y0 >= y1)
		return;
	float u0, u1;
	if (hflip) {
		u0 = (ex + s - (x0 - x) - INSET) / tw;
		u1 = (ex + s - (x1 - x) + INSET) / tw;
	} else {
		u0 = (ex + (x0 - x) + INSET) / tw;
		u1 = (ex + (x1 - x) - INSET) / tw;
	}
	float v0 = (ey + (y0 - y) + INSET) / th, v1 = (ey + (y1 - y) - INSET) / th;
	quad((float)x0, (float)y0, (float)x1, (float)y1, u0, v0, u1, v1, z);
}

void naomi_pvr_atarisys2_draw_op(void)
{
	if (!ready)
		return;
	/* the playfield: bitmap (x, y) shows texel ((x - xscroll) & 1023, (y - yscroll) & 511) */
	header(PVR_LIST_OP_POLY, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0), 1024, 512, pf_tex);
	float u0 = (float)((-scroll_x) & 1023) / 1024.0f, v0 = (float)((-scroll_y) & 511) / 512.0f;
	quad(0, 0, VIS_W, VIS_H, u0, v0, u0 + VIS_W / 1024.0f, v0 + VIS_H / 512.0f, 1.0f);
}

void naomi_pvr_atarisys2_draw_pt(void)
{
	if (!ready)
		return;
	naomi_live.pieces_mo = n_mo;
	naomi_live.pieces_pf = n_pf;
	naomi_live.pieces_al = n_al;
	int i, last;
	float z = 2.0f;
	/* motion objects, in order: each later one in front */
	last = -1;
	for (i = 0; i < n_mo; i++) {
		const struct mo_piece *m = &mos[i];
		if (m->color != last) {
			header(PVR_LIST_PT_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(BANK_MO + m->color), 1024, 512, mo_atlas);
			last = m->color;
		}
		int ex, ey;
		atlas_pos(m->pict, 16, 512, &ex, &ey);
		piece(m->x, m->y, 16, ex, ey, 1024, 512, m->hflip, 0, VIS_W - 1, m->cy0, m->cy1, z);
		z += 0.001f;
	}
	/* playfield pens 8-15 over the motion objects */
	z = 10.0f;
	last = -1;
	for (i = 0; i < n_pf; i++) {
		const struct pf_piece *p = &pfs[i];
		if (p->color != last) {
			header(PVR_LIST_PT_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(BANK_PFHI + p->color), 1024, 1024, pf_atlas);
			last = p->color;
		}
		int ex, ey;
		atlas_pos(p->tile, 8, 1024, &ex, &ey);
		piece(p->x, p->y, 8, ex, ey, 1024, 1024, 0, p->cx0, p->cx1, p->cy0, p->cy1, z);
	}
	/* alphanumerics */
	z = 20.0f;
	last = -1;
	for (i = 0; i < n_al; i++) {
		const struct al_piece *a = &als[i];
		if (a->color != last) {
			header(PVR_LIST_PT_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(BANK_AL + a->color), 256, 256, al_atlas);
			last = a->color;
		}
		int ex, ey;
		atlas_pos(a->pict, 8, 256, &ex, &ey);
		piece(a->x, a->y, 8, ex, ey, 256, 256, 0, 0, VIS_W - 1, 0, VIS_H - 1, z);
	}
}

int naomi_pvr_atarisys2_ready(void)
{
	return ready;
}

/* wait until the PVR has finished drawing the last frame, before the
   playfield texture and the palette it reads are changed */
void naomi_pvr_atarisys2_sync(void)
{
	pvr_wait_render_done();
}
