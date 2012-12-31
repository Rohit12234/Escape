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

#include <sys/common.h>
#include <sys/intrpt.h>

typedef struct {
	uint16_t ax;
	uint16_t bx;
	uint16_t cx;
	uint16_t dx;
	uint16_t si;
	uint16_t di;
	uint16_t ds;
	uint16_t es;
} sVM86Regs;

typedef struct {
	uintptr_t offset;	/* the offset to <dst> */
	uintptr_t result;	/* the address in your address-space where to copy the data */
	size_t size;		/* the size of the data */
} sVM86AreaPtr;

typedef struct {
	/* copy <src> in your process to <dst> in vm86-task before start and the other way around
	 * when finished */
	void *src;
	uintptr_t dst;
	size_t size;
	/* optionally, <ptrCount> pointers to <dst> which are copied to your address-space, that is
	 * if the result produced by the bios (at <dst>) contains pointers to other areas, you can
	 * copy the data in these areas to your address-space as well. */
	sVM86AreaPtr *ptr;
	size_t ptrCount;
} sVM86Memarea;

typedef struct {
	uint16_t interrupt;
	sVM86Regs regs;
	sVM86Memarea *area;
	void **copies;
} sVM86Info;

/**
 * Creates a vm86-task
 *
 * @return 0 on success
 */
int vm86_create(void);

/**
 * Performs a VM86-interrupt
 *
 * @param interrupt the interrupt-number
 * @param regs the register
 * @param area the memarea
 * @return 0 on success
 */
int vm86_int(uint16_t interrupt,sVM86Regs *regs,const sVM86Memarea *area);

/**
 * Handles a GPF
 *
 * @param stack the interrupt-stack-frame
 */
void vm86_handleGPF(sIntrptStackFrame *stack);
