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

#include <esc/common.h>
#include <esc/mem.h>

/* the assembler-routine */
extern u32 _mapPhysical(u32 phys,u32 count);
extern s32 _createSharedMem(const char *name,u32 byteCount);
extern s32 _joinSharedMem(const char *name);

/* just a convenience for the user because the return-value is negative if an error occurred */
/* since it will be mapped in the user-space (< 0x80000000) the MSB is never set */
void *mapPhysical(u32 phys,u32 count) {
	u32 addr = _mapPhysical(phys,count);
	if(errno < 0)
		return NULL;
	return (void*)addr;
}

void *createSharedMem(const char *name,u32 byteCount) {
	s32 err = _createSharedMem(name,byteCount);
	if(err < 0)
		return NULL;
	return (void*)err;
}

void *joinSharedMem(const char *name) {
	s32 err = _joinSharedMem(name);
	if(err < 0)
		return NULL;
	return (void*)err;
}
