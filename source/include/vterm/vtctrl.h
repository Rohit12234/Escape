/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <esc/common.h>
#include <esc/ringbuffer.h>
#include <esc/esccodes.h>
#include <esc/messages.h>
#include <esc/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VTERM_COUNT			6
#define TAB_WIDTH			4
#define HISTORY_SIZE		12
#define INPUT_BUF_SIZE		512
#define MAX_VT_NAME_LEN		15

/**
 * The handler for shortcuts
 */
typedef struct sVTerm sVTerm;
typedef bool (*fHandleShortcut)(sVTerm *vt,uchar keycode,uchar modifier,char c);
typedef void (*fSetCursor)(sVTerm *vt);

/* global configuration */
typedef struct {
	bool readKb;
	bool enabled;
} sVTermCfg;

/* our vterm-state */
struct sVTerm {
	/* identification */
	uchar index;
	int sid;
	char name[MAX_VT_NAME_LEN + 1];
	/* to lock this vterm */
	tULock lock;
	/* function-pointers */
	fSetCursor setCursor;
	/* number of cols/rows on the screen */
	size_t cols;
	size_t rows;
	/* position (on the current page) */
	size_t col;
	size_t row;
	size_t lastCol;
	size_t lastRow;
	/* colors */
	uchar defForeground;
	uchar defBackground;
	uchar foreground;
	uchar background;
	/* whether this vterm is currently active */
	uchar active;
	/* file-descriptors */
	int video;
	int speaker;
	/* the first line with content */
	size_t firstLine;
	/* the line where row+col starts */
	size_t currLine;
	/* the first visible line */
	size_t firstVisLine;
	/* a range that should be updated */
	size_t upStart;
	size_t upLength;
	ssize_t upScroll;
	/* whether entered characters should be echo'd to screen */
	uchar echo;
	/* whether the vterm should read until a newline occurrs */
	uchar readLine;
	/* whether navigation via up/down/pageup/pagedown is enabled */
	uchar navigation;
	/* whether all output should be printed into the readline-buffer */
	uchar printToRL;
	/* whether all output should be printed to COM1 */
	uchar printToCom1;
	/* a backup of the screen; initially NULL */
	char *screenBackup;
	size_t backupCol;
	size_t backupRow;
	/* the buffer for the input-stream */
	uchar inbufEOF;
	sRingBuf *inbuf;
	/* the pid of the shell for ctrl+c notifications */
	pid_t shellPid;
	/* the escape-state */
	int escapePos;
	char escapeBuf[MAX_ESCC_LENGTH];
	/* readline-buffer */
	size_t rlStartCol;
	size_t rlBufSize;
	size_t rlBufPos;
	char *rlBuffer;
	char *buffer;
	char *titleBar;
};

/* the colors */
typedef enum {
	/* 0 */ BLACK,
	/* 1 */ BLUE,
	/* 2 */ GREEN,
	/* 3 */ CYAN,
	/* 4 */ RED,
	/* 5 */ MARGENTA,
	/* 6 */ ORANGE,
	/* 7 */ LIGHTGRAY,
	/* 8 */ GRAY,
	/* 9 */ LIGHTBLUE,
	/* 10 */ LIGHTGREEN,
	/* 11 */ LIGHTCYAN,
	/* 12 */ LIGHTRED,
	/* 13 */ LIGHTMARGENTA,
	/* 14 */ YELLOW,
	/* 15 */ WHITE
} eColor;

/**
 * Inits the vterm
 *
 * @param vt the vterm
 * @param vidSize the size of the screen
 * @param vidFd the file-descriptor for the video-device (or whatever you need :))
 * @param speakerFd the file-descriptor for the speaker-device
 * @return true if successfull
 */
bool vtctrl_init(sVTerm *vt,sVTSize *vidSize,int vidFd,int speakerFd);

/**
 * Handles the control-commands
 *
 * @param vt the vterm
 * @param cfg global configuration
 * @param cmd the command
 * @param data the data
 * @return the result
 */
int vtctrl_control(sVTerm *vt,sVTermCfg *cfg,uint cmd,void *data);

/**
 * Scrolls the screen by <lines> up (positive) or down (negative) (unlocked)
 *
 * @param vt the vterm
 * @param lines the number of lines to move
 */
void vtctrl_scroll(sVTerm *vt,int lines);

/**
 * Marks the whole screen including title-bar dirty (unlocked)
 *
 * @param vt the vterm
 */
void vtctrl_markScrDirty(sVTerm *vt);

/**
 * Marks the given range as dirty (unlocked)
 *
 * @param vt the vterm
 * @param start the start-position
 * @param length the number of bytes
 */
void vtctrl_markDirty(sVTerm *vt,size_t start,size_t length);

/**
 * Releases resources (unlocked)
 *
 * @param vt the vterm
 */
void vtctrl_destroy(sVTerm *vt);

/**
 * Resizes the size of the terminal to <cols> x <rows>.
 *
 * @param vt the terminal
 * @param cols the number of columns
 * @param rows the number of rows
 * @return true if it has changed something
 */
bool vtctrl_resize(sVTerm *vt,size_t cols,size_t rows);

#ifdef __cplusplus
}
#endif
