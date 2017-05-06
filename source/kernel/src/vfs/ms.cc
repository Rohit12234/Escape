/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
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

#include <task/mntspace.h>
#include <vfs/info.h>
#include <vfs/ms.h>
#include <common.h>
#include <ostringstream.h>
#include <util.h>

VFSMS::VFSMS(pid_t pid,VFSNode *parent,uint64_t id,char *name,uint mode,bool &success)
	: VFSDir(pid,parent,name,MODE_TYPE_MOUNTSPC | mode,success), _id(id) {
	if(!success)
		return;

	VFSNode *info = createObj<VFSInfo::MountsFile>(KERNEL_PID,this);
	if(info == NULL) {
		success = false;
		return;
	}
	VFSNode::release(info);
}
