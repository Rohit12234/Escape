/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
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

#ifndef COW_H_
#define COW_H_

#include <common.h>
#include <task/proc.h>

/**
 * Initializes copy-on-write
 */
void cow_init(void);

/**
 * Handles a pagefault for given address. Assumes that the pagefault was caused by a write access
 * to a copy-on-write page!
 *
 * @param address the address
 */
void cow_pagefault(u32 address);

/**
 * Adds the given frame and process to the cow-list.
 *
 * @param p the process
 * @param frameNo the frame-number
 * @param isParent wether its the parent-process of the cow-create-progress
 * @return true if successfull
 */
bool cow_add(sProc *p,u32 frameNo,bool isParent);

/**
 * Removes the given process and frame from the cow-list
 *
 * @param p the process
 * @param frameNo the frame-number
 */
bool cow_remove(sProc *p,u32 frameNo);

#if DEBUGGING

/**
 * Prints the cow-list
 */
void cow_dbg_print(void);

#endif

#endif /* COW_H_ */
