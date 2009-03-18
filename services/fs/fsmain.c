/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include <common.h>
#include <io.h>
#include <service.h>
#include <proc.h>
#include <heap.h>
#include <debug.h>
#include <messages.h>
#include <string.h>

#include "ext2/ext2.h"
#include "ext2/path.h"
#include "ext2/inode.h"
#include "ext2/inodecache.h"
#include "ext2/file.h"
#include <fsinterface.h>

/* open-response */
typedef struct {
	sMsgDefHeader header;
	sMsgDataFSOpenResp data;
} __attribute__((packed)) sMsgOpenResp;
/* write-response */
typedef struct {
	sMsgDefHeader header;
	sMsgDataFSWriteResp data;
} __attribute__((packed)) sMsgWriteResp;

/* the message we'll send */
static sMsgOpenResp openResp = {
	.header = {
		.id = MSG_FS_OPEN_RESP,
		.length = sizeof(sMsgDataFSOpenResp)
	},
	.data = {
		.pid = 0,
		.inodeNo = 0
	}
};
static sMsgWriteResp writeResp = {
	.header = {
		.id = MSG_FS_WRITE_RESP,
		.length = sizeof(sMsgDataFSWriteResp)
	},
	.data = {
		.pid = 0,
		.count = 0
	}
};

static sExt2 ext2;

s32 main(void) {
	s32 fd,id;

	return 0;

	/* register service */
	id = regService("fs",SERVICE_TYPE_MULTIPIPE);
	if(id < 0) {
		printLastError();
		return 1;
	}

	/* TODO */
	ext2.drive = 0;
	ext2.partition = 0;
	if(!ext2_init(&ext2)) {
		unregService(id);
		return 1;
	}

	while(1) {
		fd = getClient(id);
		if(fd < 0)
			sleep();
		else {
			sMsgDefHeader header;
			while(read(fd,&header,sizeof(sMsgDefHeader)) > 0) {
				switch(header.id) {
					case MSG_FS_OPEN: {
						/* read data */
						sMsgDataFSOpenReq *data = (sMsgDataFSOpenReq*)malloc(sizeof(u8) * header.length);
						if(data != NULL) {
							/* TODO we need a way to skip a message or something.. */
							tInodeNo no;
							read(fd,data,header.length);

							no = ext2_resolvePath(&ext2,(string)(data + 1));
							if(no != EXT2_BAD_INO) {
								/*sCachedInode *cnode = ext2_icache_request(&ext2,no);
								ext2_dbg_printInode(&(cnode->inode));
								ext2_icache_release(&ext2,cnode);*/
							}

							/*debugf("Received an open from %d of '%s' for ",data->pid,data + 1);
							if(data->flags & IO_READ)
								debugf("READ");
							if(data->flags & IO_WRITE) {
								if(data->flags & IO_READ)
									debugf(" and ");
								debugf("WRITE");
							}
							debugf("\n");*/

							/* write response */
							openResp.data.pid = data->pid;
							openResp.data.inodeNo = no;
							write(fd,&openResp,sizeof(sMsgOpenResp));
							free(data);
						}
					}
					break;

					case MSG_FS_READ: {
						sMsgDefHeader *rhead;
						sMsgDataFSReadResp *rdata;
						u32 dlen;
						u32 count;

						/* read data */
						sMsgDataFSReadReq data;
						read(fd,&data,header.length);

						/* write response  */
						dlen = sizeof(sMsgDataFSReadResp) + data.count * sizeof(u8);
						rhead = (u8*)malloc(sizeof(sMsgDefHeader) + dlen);
						if(rhead != NULL) {
							rdata = (sMsgDataFSReadResp*)(rhead + 1);
							count = ext2_readFile(&ext2,data.inodeNo,(u8*)(rdata + 1),
										data.offset,data.count);

							dlen = sizeof(sMsgDataFSReadResp) + count * sizeof(u8);
							rhead->length = dlen;
							rhead->id = MSG_FS_READ_RESP;
							rdata->count = count;
							rdata->pid = data.pid;

							write(fd,rhead,sizeof(sMsgDefHeader) + dlen);
							free(rhead);
						}
					}
					break;

					case MSG_FS_WRITE: {
						/* read data */
						sMsgDataFSWriteReq *data = (sMsgDataFSWriteReq*)malloc(header.length);
						if(data != NULL) {
							read(fd,data,header.length);

							debugf("Got '%s' (%d bytes) for offset %d in inode %d\n",data + 1,
									data->count,data->offset,data->inodeNo);

							/* write response */
							writeResp.data.pid = data->pid;
							writeResp.data.count = data->count;
							write(fd,&writeResp,sizeof(sMsgWriteResp));
							free(data);
						}
					}
					break;

					case MSG_FS_CLOSE: {
						/* read data */
						sMsgDataFSCloseReq data;
						read(fd,&data,sizeof(sMsgDataFSCloseReq));

						/*debugf("Closing inode %d\n",data.inodeNo);*/
					}
					break;
				}
			}
			close(fd);
		}
	}

	/* clean up */
	unregService(id);

	return 0;
}
