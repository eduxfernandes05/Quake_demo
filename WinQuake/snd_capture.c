/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_capture.c -- headless audio capture driver
//
// Provides a fake DMA buffer that the snd_dma.c mixer writes into.
// No real audio hardware is used; playback progress is simulated
// via wall-clock time so the mixer keeps advancing.
// SND_CaptureAudio() exports the buffer contents for external use.

#include "quakedef.h"

#define CAPTURE_BUFFER_SAMPLES 16384

static short capture_buffer[CAPTURE_BUFFER_SAMPLES];
static double capture_start_time;
static qboolean capture_initialized = false;

/*
==================
SNDDMA_Init

Set up the fake DMA buffer for the mixer.
==================
*/
qboolean SNDDMA_Init(void)
{
	shm = &sn;
	shm->speed = 11025;
	shm->samplebits = 16;
	shm->channels = 2;
	shm->samples = CAPTURE_BUFFER_SAMPLES;
	shm->buffer = (unsigned char *)capture_buffer;
	shm->submission_chunk = 512;
	shm->samplepos = 0;
	shm->soundalive = true;
	shm->gamealive = true;
	shm->splitbuffer = 0;

	memset(capture_buffer, 0, sizeof(capture_buffer));
	capture_start_time = Sys_FloatTime();
	capture_initialized = true;

	return true;
}

/*
==================
SNDDMA_GetDMAPos

Return an advancing sample position based on elapsed wall-clock time.
This simulates audio hardware consuming the ring buffer so the mixer
keeps producing new data.
==================
*/
int SNDDMA_GetDMAPos(void)
{
	int pos;

	if (!capture_initialized)
		return 0;

	pos = (int)((Sys_FloatTime() - capture_start_time) * shm->speed * shm->channels);
	pos &= (shm->samples - 1);
	shm->samplepos = pos;

	return pos;
}

/*
==================
SNDDMA_Shutdown
==================
*/
void SNDDMA_Shutdown(void)
{
	capture_initialized = false;
}

/*
==================
SNDDMA_Submit

Nothing to do — the mixer writes directly into our buffer.
==================
*/
void SNDDMA_Submit(void)
{
}

/*
==================
SND_CaptureAudio

Export the current capture buffer contents for external frame capture.
Returns a pointer to the PCM data, the number of samples, and the rate.
==================
*/
void SND_CaptureAudio(short **pcm, int *samples, int *rate)
{
	if (pcm)
		*pcm = capture_buffer;
	if (samples)
		*samples = CAPTURE_BUFFER_SAMPLES;
	if (rate)
		*rate = 11025;
}
