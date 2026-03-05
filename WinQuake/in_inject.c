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
// in_inject.c -- programmatic input injection driver for headless mode
//
// Replaces hardware input with queue-based injection.
// External code pushes events via IN_InjectKeyEvent / IN_InjectMouseEvent;
// the engine consumes them through the normal IN_Commands / IN_Move path.

#include "quakedef.h"

#define INPUT_QUEUE_SIZE 256

typedef struct {
	int key;
	qboolean down;
} key_event_t;

typedef struct {
	int dx, dy;
	int buttons;
} mouse_event_t;

static key_event_t key_queue[INPUT_QUEUE_SIZE];
static int key_queue_head = 0;
static int key_queue_tail = 0;

static mouse_event_t mouse_queue[INPUT_QUEUE_SIZE];
static int mouse_queue_head = 0;
static int mouse_queue_tail = 0;

/*
==================
IN_Init
==================
*/
void IN_Init(void)
{
	key_queue_head = key_queue_tail = 0;
	mouse_queue_head = mouse_queue_tail = 0;
}

/*
==================
IN_Shutdown
==================
*/
void IN_Shutdown(void)
{
}

/*
==================
IN_Commands

Dequeue pending key events and dispatch them into the engine.
==================
*/
void IN_Commands(void)
{
	while (key_queue_tail != key_queue_head)
	{
		key_event_t *ev = &key_queue[key_queue_tail % INPUT_QUEUE_SIZE];
		Key_Event(ev->key, ev->down);
		key_queue_tail = (key_queue_tail + 1) % INPUT_QUEUE_SIZE;
	}
}

/*
==================
IN_Move

Dequeue pending mouse events and apply them to view angles.
==================
*/
void IN_Move(usercmd_t *cmd)
{
	while (mouse_queue_tail != mouse_queue_head)
	{
		mouse_event_t *ev = &mouse_queue[mouse_queue_tail % INPUT_QUEUE_SIZE];

		cl.viewangles[YAW] -= m_yaw.value * ev->dx;
		cl.viewangles[PITCH] += m_pitch.value * ev->dy;

		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;

		mouse_queue_tail = (mouse_queue_tail + 1) % INPUT_QUEUE_SIZE;
	}
}

/*
==================
Sys_SendKeyEvents

Called by the engine to poll for pending input.
In headless mode there is no OS event source; everything
arrives through IN_InjectKeyEvent instead.
==================
*/
void Sys_SendKeyEvents(void)
{
}

/*
==================
IN_InjectKeyEvent

Push a key event into the queue for the engine to consume.
==================
*/
void IN_InjectKeyEvent(int key, qboolean down)
{
	int next = (key_queue_head + 1) % INPUT_QUEUE_SIZE;
	if (next == key_queue_tail)
		return; /* queue full — drop event */

	key_queue[key_queue_head].key = key;
	key_queue[key_queue_head].down = down;
	key_queue_head = next;
}

/*
==================
IN_InjectMouseEvent

Push a mouse event into the queue for the engine to consume.
==================
*/
void IN_InjectMouseEvent(int dx, int dy, int buttons)
{
	int next = (mouse_queue_head + 1) % INPUT_QUEUE_SIZE;
	if (next == mouse_queue_tail)
		return; /* queue full — drop event */

	mouse_queue[mouse_queue_head].dx = dx;
	mouse_queue[mouse_queue_head].dy = dy;
	mouse_queue[mouse_queue_head].buttons = buttons; /* reserved for future use */
	mouse_queue_head = next;
}
