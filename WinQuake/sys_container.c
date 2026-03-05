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
// sys_container.c — Container-ready system layer for headless builds
//
// Replaces sys_linux.c when building for containerized deployment.
// Features:
//   - SIGTERM handler for graceful shutdown
//   - Configuration via environment variables (QUAKE_BASEDIR, QUAKE_MAP, QUAKE_SKILL)
//   - Structured JSON output to stdout
//   - Minimal HTTP health endpoint on /healthz

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#include "quakedef.h"

qboolean		isDedicated;

static volatile sig_atomic_t host_shutdown_flag = 0;
static qboolean host_frame_running = false;

/* Default basedir — overridden by QUAKE_BASEDIR env var */
char *basedir = ".";
char *cachedir = "/tmp";

cvar_t  sys_linerefresh = {"sys_linerefresh","0"};

/* Health endpoint port — overridden by QUAKE_HEALTH_PORT env var */
#define DEFAULT_HEALTH_PORT 8080
static int health_port = DEFAULT_HEALTH_PORT;
static int health_listen_fd = -1;

// =======================================================================
// SIGTERM handler
// =======================================================================

static void sigterm_handler(int sig)
{
	(void)sig;
	host_shutdown_flag = 1;
}

// =======================================================================
// JSON output helpers
// =======================================================================

static void json_log(const char *level, const char *message)
{
	struct timespec ts;
	struct tm tm_buf;
	char timebuf[64];

	clock_gettime(CLOCK_REALTIME, &ts);
	gmtime_r(&ts.tv_sec, &tm_buf);
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tm_buf);

	fprintf(stdout, "{\"time\":\"%s.%03ldZ\",\"level\":\"%s\",\"msg\":",
		timebuf, ts.tv_nsec / 1000000, level);

	/* Emit the message as a JSON string, escaping special chars */
	fputc('"', stdout);
	for (const char *p = message; *p; p++) {
		unsigned char c = (unsigned char)*p;
		switch (c) {
		case '"':  fputs("\\\"", stdout); break;
		case '\\': fputs("\\\\", stdout); break;
		case '\n': fputs("\\n", stdout);  break;
		case '\r': fputs("\\r", stdout);  break;
		case '\t': fputs("\\t", stdout);  break;
		default:
			if (c >= 0x20 && c < 0x7f)
				fputc(c, stdout);
			else
				fprintf(stdout, "\\u%04x", c);
			break;
		}
	}
	fputc('"', stdout);

	fputs("}\n", stdout);
	fflush(stdout);
}

// =======================================================================
// General routines
// =======================================================================

void Sys_DebugNumber(int y, int val)
{
}

void Sys_Printf(char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start(argptr, fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	/* Strip high-bit Quake chars for clean output */
	for (unsigned char *p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			*p = '.';
	}

	json_log("info", text);
}

void Sys_Quit(void)
{
	Host_Shutdown();
	if (health_listen_fd >= 0)
		close(health_listen_fd);
	json_log("info", "Engine shutdown complete");
	fflush(stdout);
	exit(0);
}

void Sys_Init(void)
{
#if id386
	Sys_SetFPCW();
#endif
}

void Sys_Error(char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	json_log("error", string);

	Host_Shutdown();
	if (health_listen_fd >= 0)
		close(health_listen_fd);
	exit(1);
}

void Sys_Warn(char *warning, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start(argptr, warning);
	vsnprintf(string, sizeof(string), warning, argptr);
	va_end(argptr);

	json_log("warn", string);
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime(char *path)
{
	struct	stat	buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

void Sys_mkdir(char *path)
{
	mkdir(path, 0777);
}

int Sys_FileOpenRead(char *path, int *handle)
{
	int	h;
	struct stat	fileinfo;

	h = open(path, O_RDONLY, 0666);
	*handle = h;
	if (h == -1)
		return -1;

	if (fstat(h, &fileinfo) == -1)
		Sys_Error("Error fstating %s", path);

	return fileinfo.st_size;
}

int Sys_FileOpenWrite(char *path)
{
	int     handle;

	umask(0);

	handle = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);

	if (handle == -1)
		Sys_Error("Error opening %s: %s", path, strerror(errno));

	return handle;
}

int Sys_FileWrite(int handle, void *src, int count)
{
	return write(handle, src, count);
}

void Sys_FileClose(int handle)
{
	close(handle);
}

void Sys_FileSeek(int handle, int position)
{
	lseek(handle, position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
	return read(handle, dest, count);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	static char data[1024];
	int fd;

	va_start(argptr, fmt);
	vsnprintf(data, sizeof(data), fmt, argptr);
	va_end(argptr);
	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd >= 0) {
		write(fd, data, strlen(data));
		close(fd);
	}
}

void Sys_EditFile(char *filename)
{
	/* Not supported in container mode */
}

double Sys_FloatTime(void)
{
	struct timeval tp;
	struct timezone tzp;
	static int      secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

void Sys_LineRefresh(void)
{
}

void floating_point_exception_handler(int whatever)
{
	signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
	return NULL;	/* No interactive console in container mode */
}

#if !id386
void Sys_HighFPPrecision(void)
{
}

void Sys_LowFPPrecision(void)
{
}
#endif

// =======================================================================
// Health endpoint — minimal HTTP server
// =======================================================================

static int health_setup(int port)
{
	int fd;
	int opt = 1;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	/* Set non-blocking so accept() never stalls the game loop */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((unsigned short)port);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}

	if (listen(fd, 4) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void health_poll(void)
{
	int client_fd;
	char buf[512];
	ssize_t n;

	if (health_listen_fd < 0)
		return;

	client_fd = accept(health_listen_fd, NULL, NULL);
	if (client_fd < 0)
		return;

	/* Read (and discard) the request */
	n = read(client_fd, buf, sizeof(buf) - 1);
	if (n <= 0) {
		close(client_fd);
		return;
	}
	buf[n] = '\0';

	/* Check if this is a GET /healthz request */
	if (strstr(buf, "GET /healthz") != NULL && host_frame_running) {
		const char *resp =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: application/json\r\n"
			"Connection: close\r\n"
			"Content-Length: 15\r\n"
			"\r\n"
			"{\"status\":\"ok\"}";
		write(client_fd, resp, strlen(resp));
	} else if (strstr(buf, "GET /healthz") != NULL) {
		const char *resp =
			"HTTP/1.1 503 Service Unavailable\r\n"
			"Content-Type: application/json\r\n"
			"Connection: close\r\n"
			"Content-Length: 24\r\n"
			"\r\n"
			"{\"status\":\"unavailable\"}";
		write(client_fd, resp, strlen(resp));
	} else {
		const char *resp =
			"HTTP/1.1 404 Not Found\r\n"
			"Connection: close\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
		write(client_fd, resp, strlen(resp));
	}

	close(client_fd);
}

// =======================================================================
// main — container entry point
// =======================================================================

int main(int argc, char **argv)
{
	double		time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;
	int j;
	const char *env_val;

	/* Install signal handlers */
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigterm_handler);
	signal(SIGFPE, SIG_IGN);

	memset(&parms, 0, sizeof(parms));

	COM_InitArgv(argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 8 * 1024 * 1024;

	j = COM_CheckParm("-mem");
	if (j)
		parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);
	parms.membase = malloc(parms.memsize);

	/* Read basedir from environment, fall back to command line / default */
	env_val = getenv("QUAKE_BASEDIR");
	if (env_val && env_val[0])
		basedir = (char *)env_val;
	parms.basedir = basedir;

	/* Health endpoint port */
	env_val = getenv("QUAKE_HEALTH_PORT");
	if (env_val && env_val[0])
		health_port = atoi(env_val);
	if (health_port <= 0 || health_port > 65535)
		health_port = DEFAULT_HEALTH_PORT;

	/* Start health endpoint */
	health_listen_fd = health_setup(health_port);
	if (health_listen_fd >= 0) {
		char msg[128];
		snprintf(msg, sizeof(msg), "Health endpoint listening on port %d", health_port);
		json_log("info", msg);
	} else {
		json_log("warn", "Failed to start health endpoint");
	}

	json_log("info", "Initializing Quake engine");

	Host_Init(&parms);
	Sys_Init();

	/* Apply environment-based configuration after engine init */
	env_val = getenv("QUAKE_SKILL");
	if (env_val && env_val[0]) {
		char cmd_buf[64];
		snprintf(cmd_buf, sizeof(cmd_buf), "skill %s\n", env_val);
		Cbuf_AddText(cmd_buf);
	}

	env_val = getenv("QUAKE_MAP");
	if (env_val && env_val[0]) {
		char cmd_buf[128];
		snprintf(cmd_buf, sizeof(cmd_buf), "map %s\n", env_val);
		Cbuf_AddText(cmd_buf);
	}

	json_log("info", "Quake engine initialized — entering main loop");
	host_frame_running = true;

	oldtime = Sys_FloatTime() - 0.1;
	while (!host_shutdown_flag)
	{
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated)
		{
			if (time < sys_ticrate.value && (vcrFile == -1 || recording))
			{
				usleep(1000);
				continue;
			}
			time = sys_ticrate.value;
		}

		if (time > sys_ticrate.value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);

		/* Poll health endpoint (non-blocking) */
		health_poll();
	}

	/* Graceful shutdown triggered by SIGTERM/SIGINT */
	json_log("info", "Shutdown signal received — cleaning up");
	host_frame_running = false;
	Host_Shutdown();
	if (health_listen_fd >= 0)
		close(health_listen_fd);
	json_log("info", "Engine shutdown complete");
	fflush(stdout);

	return 0;
}

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize - 1)) - psize;

	r = mprotect((char *)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
		Sys_Error("Protection change failed\n");
}
