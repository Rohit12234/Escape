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

#include <esc/common.h>
#include <esc/driver.h>
#include <esc/debug.h>
#include <esc/messages.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "list.h"

static sMsg msg;

int main(void) {
	msgid_t mid;
	int id;

	list_init();

	id = createdev("/dev/pci",DEV_TYPE_SERVICE,0);
	if(id < 0)
		error("Unable to register device 'pci'");
	if(chmod("/dev/pci",0110) < 0)
		error("Unable to change permissions of /dev/pci");

	while(1) {
		int fd = getwork(id,&mid,&msg,sizeof(msg),0);
		if(fd < 0)
			printe("Unable to get work");
		else {
			switch(mid) {
				case MSG_PCI_GET_BY_CLASS: {
					uchar baseClass = (uchar)msg.args.arg1;
					uchar subClass = (uchar)msg.args.arg2;
					sPCIDevice *d = list_getByClass(baseClass,subClass);
					msg.data.arg1 = -1;
					if(d) {
						msg.data.arg1 = sizeof(sPCIDevice);
						memcpy(msg.data.d,d,sizeof(sPCIDevice));
					}
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.data));
				}
				break;
				case MSG_PCI_GET_BY_ID: {
					uchar bus = (uchar)msg.args.arg1;
					uchar dev = (uchar)msg.args.arg2;
					uchar func = (uchar)msg.args.arg3;
					sPCIDevice *d = list_getById(bus,dev,func);
					msg.data.arg1 = -1;
					if(d) {
						msg.data.arg1 = sizeof(sPCIDevice);
						memcpy(msg.data.d,d,sizeof(sPCIDevice));
					}
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.data));
				}
				break;
				case MSG_PCI_GET_LIST: {
					size_t idx = (size_t)msg.args.arg1;
					if(idx == (size_t)-1)
						msg.data.arg1 = list_length();
					else {
						sPCIDevice *dev = list_get(idx);
						if(dev) {
							msg.data.arg1 = sizeof(sPCIDevice);
							memcpy(msg.data.d,dev,sizeof(sPCIDevice));
						}
						else
							msg.data.arg1 = -EINVAL;
					}
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.data));
				}
				break;
				default:
					msg.args.arg1 = -ENOTSUP;
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.args));
					break;
			}
		}
	}

	close(id);
	return EXIT_SUCCESS;
}
