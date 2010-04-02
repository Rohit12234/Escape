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

#include <common.h>
#include <task/proc.h>
#include <task/elf.h>
#include <mem/paging.h>
#include <mem/pmem.h>
#include <mem/vmm.h>
#include <vfs/vfs.h>
#include <vfs/real.h>
#include <video.h>
#include <string.h>
#include <errors.h>
#include <assert.h>

static s32 elf_addSegment(sBinDesc *bindesc,Elf32_Phdr *pheader,u32 loadSegNo);

s32 elf_loadFromFile(char *path) {
	sThread *t = thread_getRunning();
	tFileNo file;
	s32 res;
	u32 j,loadSeg = 0;
	u8 const *datPtr;
	Elf32_Ehdr eheader;
	Elf32_Phdr pheader;
	sFileInfo info;
	sBinDesc bindesc;
	tInodeNo ino;
	tDevNo dev;

	file = vfsr_openFile(t->tid,VFS_READ,path);
	if(file < 0)
		return ERR_INVALID_ELF_BIN;

	/* fill bindesc */
	if(vfs_getFileId(file,&ino,&dev) < 0)
		goto failed;
	if(vfsr_istat(t->tid,ino,dev,&info) < 0)
		goto failed;
	bindesc.path = path;
	if(bindesc.path == NULL)
		goto failed;
	bindesc.modifytime = info.modifytime;

	/* first read the header */
	if((res = vfs_readFile(t->tid,file,(u8*)&eheader,sizeof(Elf32_Ehdr))) < 0)
		goto failed;

	/* check magic */
	if(eheader.e_ident.dword != *(u32*)ELFMAG)
		goto failed;

	/* load the LOAD segments. */
	datPtr = (u8 const*)(eheader.e_phoff);
	for(j = 0; j < eheader.e_phnum; datPtr += eheader.e_phentsize, j++) {
		/* go to header */
		if(vfs_seek(t->tid,file,(u32)datPtr,SEEK_SET) < 0)
			goto failed;
		/* read pheader */
		res = vfs_readFile(t->tid,file,(u8*)&pheader,sizeof(Elf32_Phdr));
		if(res < 0 || res != sizeof(Elf32_Phdr))
			goto failed;

		if(pheader.p_type == PT_LOAD) {
			if(elf_addSegment(&bindesc,&pheader,loadSeg) < 0)
				return ERR_INVALID_ELF_BIN;
			loadSeg++;
		}
	}

	vfs_closeFile(t->tid,file);
	return (u32)eheader.e_entry;

failed:
	vfs_closeFile(t->tid,file);
	return ERR_INVALID_ELF_BIN;
}

s32 elf_loadFromMem(u8 *code,u32 length) {
	u32 loadSegNo = 0;
	u32 j;
	u8 const *datPtr;
	Elf32_Ehdr *eheader = (Elf32_Ehdr*)code;
	Elf32_Phdr *pheader = NULL;

	/* check magic */
	if(eheader->e_ident.dword != *(u32*)ELFMAG)
		return ERR_INVALID_ELF_BIN;

	/* load the LOAD segments. */
	datPtr = (u8 const*)(code + eheader->e_phoff);
	for(j = 0; j < eheader->e_phnum; datPtr += eheader->e_phentsize, j++) {
		pheader = (Elf32_Phdr*)datPtr;
		/* check if all stuff is in the binary */
		if((u8*)pheader + sizeof(Elf32_Phdr) >= code + length)
			return ERR_INVALID_ELF_BIN;

		if(pheader->p_type == PT_LOAD) {
			if(pheader->p_vaddr + pheader->p_filesz >= (u32)(code + length))
				return ERR_INVALID_ELF_BIN;
			if(elf_addSegment(NULL,pheader,loadSegNo) < 0)
				return ERR_INVALID_ELF_BIN;
			/* copy the data; we zero on demand */
			memcpy((void*)pheader->p_vaddr,code + pheader->p_offset,pheader->p_filesz);
			loadSegNo++;
		}
	}

	return (u32)eheader->e_entry;
}

static s32 elf_addSegment(sBinDesc *bindesc,Elf32_Phdr *pheader,u32 loadSegNo) {
	sProc *p = proc_getRunning();
	u8 type;
	s32 res;
	/* determine type */
	if(loadSegNo == 0) {
		if(pheader->p_flags != (PF_X | PF_R) || pheader->p_vaddr != TEXT_BEGIN)
			return ERR_INVALID_ELF_BIN;
		type = REG_TEXT;
	}
	else if(pheader->p_flags == PF_R)
		type = REG_RODATA;
	else if(pheader->p_flags == (PF_R | PF_W)) {
		if(pheader->p_filesz == 0)
			type = REG_BSS;
		else
			type = REG_DATA;
	}
	else
		return ERR_INVALID_ELF_BIN;

	/* check if the sizes are valid */
	if(pheader->p_filesz > pheader->p_memsz)
		return ERR_INVALID_ELF_BIN;

	/* bss needs no binary */
	if(type == REG_BSS)
		bindesc = NULL;
	/* add the region */
	if((res = vmm_add(p,bindesc,pheader->p_offset,pheader->p_memsz,type)) < 0)
		return res;
	return 0;
}
