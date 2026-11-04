/* NAOMI sound: what audio.cpp did with SDL_mixer on the Dreamcast build.
   sound.cpp mixes one frame (sound_slice mono 16-bit samples) into
   mixedAudio; gp2x_sound_play copies it into a ring buffer that a KOS
   snd_stream (AICA) plays at sound_slice * frames per second. The stream is
   polled from the emulation thread once a frame. When emulation runs slow
   the ring runs dry and the stream plays silence; when it gets too full
   (the display clock and the AICA clock drift apart) the oldest samples
   are dropped. */

#include <kos.h>
#include <dc/sound/stream.h>
#include <string.h>

#include "driver.h"
#include "minimal.h"
#include "naomi.h"

extern int sound_slice;
extern int sound_real_sample_rate;
extern int sound_real_sample_rate_min;
extern int sound_real_sample_rate_max;
extern unsigned int soundcard;

#define RING 16384                 /* samples, power of two */
#define RING_MASK (RING - 1)
#define RING_HIGH 4096            /* about 180 ms */

static short ring[RING] __attribute__((aligned(32)));
static volatile unsigned ring_rd = 0, ring_wr = 0;
static short out_buf[8192] __attribute__((aligned(32)));
static short mix_buf[4096] __attribute__((aligned(32)));

short *mixedAudio = mix_buf;
int closing_sound = 1;

static snd_stream_hnd_t stream = SND_STREAM_INVALID;
static int stream_rate = 0;
static int muted = 0;

static void *stream_cb(snd_stream_hnd_t hnd, int bytes_req, int *bytes_recv)
{
	(void)hnd;
	int n = bytes_req >> 1;
	if (n > (int)(sizeof(out_buf) / 2))
		n = sizeof(out_buf) / 2;
	unsigned avail = ring_wr - ring_rd;
	int i = 0;
	if (!muted) {
		for (; i < n && avail; i++, avail--) {
			out_buf[i] = ring[ring_rd & RING_MASK];
			ring_rd++;
		}
	}
	for (; i < n; i++)
		out_buf[i] = 0;
	*bytes_recv = n << 1;
	return out_buf;
}

static void stream_stop(void)
{
	if (stream != SND_STREAM_INVALID) {
		snd_stream_stop(stream);
		snd_stream_destroy(stream);
		stream = SND_STREAM_INVALID;
	}
}

static void stream_start(int rate)
{
	static int sys_up = 0;
	if (!sys_up) {
		snd_stream_init();
		sys_up = 1;
	}
	if (stream != SND_STREAM_INVALID && rate == stream_rate)
		return;
	stream_stop();
	ring_rd = ring_wr = 0;
	stream = snd_stream_alloc(stream_cb, 8192);
	if (stream == SND_STREAM_INVALID)
		return;
	snd_stream_volume(stream, 255);
	snd_stream_start(stream, rate, 0);
	stream_rate = rate;
}

/* Called by sound.cpp (msdos_init_sound) once the game's frame rate is known. */
void mame4all_init_sound_slice(void)
{
	if (sound_slice & 31)
		sound_slice += 32 - (sound_slice & 31);
	sound_real_sample_rate = sound_slice * Machine->drv->frames_per_second;
	sound_real_sample_rate_min = sound_real_sample_rate - 16;
	sound_real_sample_rate_max = sound_real_sample_rate + 16;
	if (soundcard)
		stream_start(sound_real_sample_rate);
}

void gp2x_sound_play(void *buff, int len)
{
	const short *s = (const short *)buff;
	int n = len >> 1;
	closing_sound = 0;
	if (stream == SND_STREAM_INVALID)
		return;
	unsigned wr = ring_wr;
	for (int i = 0; i < n; i++)
		ring[(wr + i) & RING_MASK] = s[i];
	ring_wr = wr + n;
	if (ring_wr - ring_rd > RING_HIGH)
		ring_rd = ring_wr - 1536;
	snd_stream_poll(stream);
}

void osd_stop_music(void) { }
void osd_start_music(void) { }
void osd_mute_sound(void) { muted = 1; }
void osd_unmute_sound(void) { muted = 0; }

void naomi_audio_quit(void)
{
	stream_stop();
}
