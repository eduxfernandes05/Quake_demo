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
// vid_headless.c — Headless video driver using Mesa EGL + LLVMpipe
//
// Provides off-screen rendering via EGL/GBM with an FBO render target.
// The software renderer writes to vid.buffer (8-bit indexed), and
// VID_Update() converts frames to RGBA via GL for capture.
// VID_CaptureFrame() exports the RGBA framebuffer for external use.

#include "quakedef.h"
#include "d_local.h"

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <gbm.h>

viddef_t	vid;				// global video state

#define	BASEWIDTH	320
#define	BASEHEIGHT	200

static byte	vid_buffer[BASEWIDTH * BASEHEIGHT];
static short	zbuffer[BASEWIDTH * BASEHEIGHT];
static byte	surfcache[256 * 1024];

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

/* ---- EGL state ---- */
static int		gbm_fd = -1;
static struct gbm_device *gbm_dev;
static EGLDisplay	egl_display = EGL_NO_DISPLAY;
static EGLContext	egl_context = EGL_NO_CONTEXT;
static EGLSurface	egl_surface = EGL_NO_SURFACE;
static qboolean		egl_initialized = false;

/* ---- FBO state ---- */
static GLuint	fbo_id;
static GLuint	fbo_texture;

/* ---- GL extension function pointers (FBO) ---- */
static PFNGLGENFRAMEBUFFERSPROC		qglGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC	qglDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC		qglBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC	qglFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC	qglCheckFramebufferStatus;

/* ---- Capture state ---- */
static uint8_t	*capture_buffer;
static int	capture_width  = BASEWIDTH;
static int	capture_height = BASEHEIGHT;

/* ---- Palette (RGB, 256 entries × 3 bytes) ---- */
static unsigned char	current_palette[768];


/*
================
VID_LoadFBOProcs

Load GL framebuffer object function pointers via eglGetProcAddress.
================
*/
static qboolean VID_LoadFBOProcs (void)
{
	qglGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)
		eglGetProcAddress("glGenFramebuffers");
	qglDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)
		eglGetProcAddress("glDeleteFramebuffers");
	qglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)
		eglGetProcAddress("glBindFramebuffer");
	qglFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)
		eglGetProcAddress("glFramebufferTexture2D");
	qglCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)
		eglGetProcAddress("glCheckFramebufferStatus");

	if (!qglGenFramebuffers || !qglDeleteFramebuffers ||
	    !qglBindFramebuffer || !qglFramebufferTexture2D ||
	    !qglCheckFramebufferStatus)
	{
		Con_Printf("VID_LoadFBOProcs: missing FBO functions\n");
		return false;
	}
	return true;
}


/*
================
VID_InitEGL

Initialize EGL with GBM platform for headless off-screen rendering.
Falls back to EGL_DEFAULT_DISPLAY if no render node is available.
================
*/
static qboolean VID_InitEGL (void)
{
	EGLint major, minor;
	EGLConfig config;
	EGLint num_configs;

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_ALPHA_SIZE,      8,
		EGL_DEPTH_SIZE,      24,
		EGL_NONE
	};

	static const EGLint pbuffer_attribs[] = {
		EGL_WIDTH,  BASEWIDTH,
		EGL_HEIGHT, BASEHEIGHT,
		EGL_NONE
	};

	static const EGLint context_attribs[] = {
		EGL_NONE
	};

	/* Try GBM render node first */
	gbm_fd = open("/dev/dri/renderD128", O_RDWR);
	if (gbm_fd < 0)
		gbm_fd = open("/dev/dri/card0", O_RDWR);

	if (gbm_fd >= 0)
	{
		gbm_dev = gbm_create_device(gbm_fd);
		if (gbm_dev)
		{
			egl_display = eglGetPlatformDisplay(
				EGL_PLATFORM_GBM_KHR,
				gbm_dev, NULL);
		}
	}

	/* Fallback to default display */
	if (egl_display == EGL_NO_DISPLAY)
		egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (egl_display == EGL_NO_DISPLAY)
	{
		Con_Printf("VID_InitEGL: no EGL display available\n");
		return false;
	}

	if (!eglInitialize(egl_display, &major, &minor))
	{
		Con_Printf("VID_InitEGL: eglInitialize failed\n");
		return false;
	}
	Con_Printf("EGL %d.%d initialized\n", major, minor);

	if (!eglBindAPI(EGL_OPENGL_API))
	{
		Con_Printf("VID_InitEGL: eglBindAPI(EGL_OPENGL_API) failed\n");
		return false;
	}

	if (!eglChooseConfig(egl_display, config_attribs,
			     &config, 1, &num_configs) || num_configs == 0)
	{
		Con_Printf("VID_InitEGL: no suitable EGL config\n");
		return false;
	}

	egl_surface = eglCreatePbufferSurface(egl_display, config,
					      pbuffer_attribs);

	egl_context = eglCreateContext(egl_display, config,
				       EGL_NO_CONTEXT, context_attribs);
	if (egl_context == EGL_NO_CONTEXT)
	{
		Con_Printf("VID_InitEGL: eglCreateContext failed\n");
		return false;
	}

	if (!eglMakeCurrent(egl_display,
			    egl_surface != EGL_NO_SURFACE
				? egl_surface : EGL_NO_SURFACE,
			    egl_surface != EGL_NO_SURFACE
				? egl_surface : EGL_NO_SURFACE,
			    egl_context))
	{
		Con_Printf("VID_InitEGL: eglMakeCurrent failed\n");
		return false;
	}

	Con_Printf("GL_RENDERER: %s\n", (const char *)glGetString(GL_RENDERER));
	Con_Printf("GL_VERSION:  %s\n", (const char *)glGetString(GL_VERSION));

	egl_initialized = true;
	return true;
}


/*
================
VID_InitFBO

Create a framebuffer object with a color texture attachment for capture.
================
*/
static qboolean VID_InitFBO (void)
{
	GLenum status;

	if (!VID_LoadFBOProcs())
		return false;

	/* Color texture */
	glGenTextures(1, &fbo_texture);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
		     capture_width, capture_height, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* FBO */
	qglGenFramebuffers(1, &fbo_id);
	qglBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
	qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, fbo_texture, 0);

	status = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		Con_Printf("VID_InitFBO: framebuffer incomplete (0x%x)\n",
			   (unsigned)status);
		qglBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}

	/* Leave FBO bound as the active render target */
	Con_Printf("FBO %ux%u created (texture %u)\n",
		   capture_width, capture_height, fbo_texture);
	return true;
}


/*
================
VID_SetPalette
================
*/
void VID_SetPalette (unsigned char *palette)
{
	int i;
	unsigned r, g, b;

	if (!palette)
		return;

	memcpy(current_palette, palette, 768);

	/* Build d_8to24table (0xAABBGGRR on little-endian) */
	for (i = 0; i < 256; i++)
	{
		r = palette[i * 3 + 0];
		g = palette[i * 3 + 1];
		b = palette[i * 3 + 2];
		d_8to24table[i] = (255u << 24) | (b << 16) | (g << 8) | r;
	}
}


/*
================
VID_ShiftPalette
================
*/
void VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette(palette);
}


/*
================
VID_Init
================
*/
void VID_Init (unsigned char *palette)
{
	/* Software renderer video state */
	vid.maxwarpwidth  = vid.width  = vid.conwidth  = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect        = 1.0;
	vid.numpages      = 1;
	vid.colormap      = host_colormap;
	vid.fullbright    = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.buffer        = vid.conbuffer = vid_buffer;
	vid.rowbytes      = vid.conrowbytes = BASEWIDTH;

	d_pzbuffer = zbuffer;
	D_InitCaches(surfcache, sizeof(surfcache));

	VID_SetPalette(palette);

	/* Initialize EGL + FBO for frame capture */
	if (VID_InitEGL())
	{
		if (!VID_InitFBO())
			Con_Printf("VID_Init: FBO init failed; "
				   "capture unavailable\n");

		/* Allocate RGBA capture buffer */
		capture_buffer = (uint8_t *)malloc(
			(size_t)capture_width * capture_height * 4);
		if (capture_buffer)
			memset(capture_buffer, 0,
			       (size_t)capture_width * capture_height * 4);
	}
	else
	{
		Con_Printf("VID_Init: EGL init failed; "
			   "running without capture\n");
	}
}


/*
================
VID_Shutdown
================
*/
void VID_Shutdown (void)
{
	if (capture_buffer)
	{
		free(capture_buffer);
		capture_buffer = NULL;
	}

	if (egl_initialized)
	{
		if (fbo_id && qglDeleteFramebuffers)
		{
			qglBindFramebuffer(GL_FRAMEBUFFER, 0);
			qglDeleteFramebuffers(1, &fbo_id);
			fbo_id = 0;
		}
		if (fbo_texture)
		{
			glDeleteTextures(1, &fbo_texture);
			fbo_texture = 0;
		}

		eglMakeCurrent(egl_display,
			       EGL_NO_SURFACE, EGL_NO_SURFACE,
			       EGL_NO_CONTEXT);

		if (egl_context != EGL_NO_CONTEXT)
			eglDestroyContext(egl_display, egl_context);
		if (egl_surface != EGL_NO_SURFACE)
			eglDestroySurface(egl_display, egl_surface);
		if (egl_display != EGL_NO_DISPLAY)
			eglTerminate(egl_display);

		egl_context = EGL_NO_CONTEXT;
		egl_surface = EGL_NO_SURFACE;
		egl_display = EGL_NO_DISPLAY;
		egl_initialized = false;
	}

	if (gbm_dev)
	{
		gbm_device_destroy(gbm_dev);
		gbm_dev = NULL;
	}
	if (gbm_fd >= 0)
	{
		close(gbm_fd);
		gbm_fd = -1;
	}
}


/*
================
VID_Update

Convert the software-rendered 8-bit frame to RGBA, upload to FBO,
and read back via glReadPixels for capture.
================
*/
void VID_Update (vrect_t *rects)
{
	int		i, npixels;
	const byte	*src;
	uint8_t		*dst;

	if (!egl_initialized || !capture_buffer || !fbo_id)
		return;

	npixels = capture_width * capture_height;
	src = vid.buffer;
	dst = capture_buffer;

	/* Convert 8-bit indexed → RGBA using d_8to24table */
	for (i = 0; i < npixels; i++)
	{
		unsigned c = d_8to24table[src[i]];
		dst[0] = (uint8_t)(c & 0xFF);           /* R */
		dst[1] = (uint8_t)((c >> 8) & 0xFF);    /* G */
		dst[2] = (uint8_t)((c >> 16) & 0xFF);   /* B */
		dst[3] = 255;                            /* A */
		dst += 4;
	}

	/* Upload to FBO texture */
	qglBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			capture_width, capture_height,
			GL_RGBA, GL_UNSIGNED_BYTE, capture_buffer);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Read back from FBO into capture buffer */
	glReadPixels(0, 0, capture_width, capture_height,
		     GL_RGBA, GL_UNSIGNED_BYTE, capture_buffer);

	glFinish();
}


/*
================
VID_CaptureFrame

Export the most recent rendered frame as RGBA pixel data.
Returns true if a frame is available.
================
*/
qboolean VID_CaptureFrame (uint8_t **rgba, int *width, int *height)
{
	if (!capture_buffer || !egl_initialized)
		return false;

	if (rgba)
		*rgba   = capture_buffer;
	if (width)
		*width  = capture_width;
	if (height)
		*height = capture_height;

	return true;
}


/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}
