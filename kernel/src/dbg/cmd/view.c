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

#include <sys/common.h>
#include <sys/dbg/console.h>
#include <sys/dbg/cmd/view.h>
#include <sys/task/proc.h>
#include <sys/task/sched.h>
#include <sys/task/signals.h>
#include <sys/task/thread.h>
#include <sys/vfs/node.h>
#include <sys/vfs/vfs.h>
#include <sys/mem/cow.h>
#include <sys/mem/kheap.h>
#include <sys/mem/paging.h>
#include <sys/mem/pmem.h>
#include <sys/mem/sharedmem.h>
#include <sys/mem/vmm.h>
#include <sys/machine/cpu.h>
#include <sys/machine/gdt.h>
#include <sys/machine/timer.h>
#include <sys/kevent.h>
#include <sys/multiboot.h>
#include <string.h>
#include <errors.h>

#define MAX_VIEWNAME_LEN	16

typedef void (*fView)(s32 argc,char **argv);
typedef struct {
	char name[MAX_VIEWNAME_LEN];
	fView func;
} sView;

static void view_printc(char c);
static void view_proc(s32 argc,char **argv);
static void view_procs(void);
static void view_sched(void);
static void view_signals(void);
static void view_thread(s32 argc,char **argv);
static void view_threads(void);
static void view_vfstree(void);
static void view_gft(void);
static void view_cow(void);
static void view_kheap(void);
static void view_pdirall(s32 argc,char **argv);
static void view_pdiruser(s32 argc,char **argv);
static void view_pdirkernel(s32 argc,char **argv);
static void view_regions(s32 argc,char **argv);
static void view_pmem(void);
static void view_pmemcont(void);
static void view_pmemstack(void);
static void view_shm(void);
static void view_cpu(void);
static void view_gdt(void);
static void view_timer(void);
static void view_kevents(void);
static void view_multiboot(void);

static sProc *view_getProc(s32 argc,char **argv);
static sThread *view_getThread(s32 argc,char **argv);

static s32 err = 0;
static sLines lines;
static sScreenBackup backup;
static sView views[] = {
	{"proc",(fView)view_proc},
	{"procs",(fView)view_procs},
	{"sched",(fView)view_sched},
	{"signals",(fView)view_signals},
	{"thread",(fView)view_thread},
	{"threads",(fView)view_threads},
	{"vfstree",(fView)view_vfstree},
	{"gft",(fView)view_gft},
	{"cow",(fView)view_cow},
	{"kheap",(fView)view_kheap},
	{"pdirall",(fView)view_pdirall},
	{"pdiruser",(fView)view_pdiruser},
	{"pdirkernel",(fView)view_pdirkernel},
	{"regions",(fView)view_regions},
	{"pmem",(fView)view_pmem},
	{"pmemcont",(fView)view_pmemcont},
	{"pmemstack",(fView)view_pmemstack},
	{"shm",(fView)view_shm},
	{"cpu",(fView)view_cpu},
	{"gdt",(fView)view_gdt},
	{"timer",(fView)view_timer},
	{"kevents",(fView)view_kevents},
	{"multiboot",(fView)view_multiboot},
};

s32 cons_cmd_view(s32 argc,char **argv) {
	u32 i;
	s32 res;
	if(argc < 2) {
		vid_printf("Usage: %s <what>\n",argv[0]);
		vid_printf("Available 'whats':\n");
		for(i = 0; i < ARRAY_SIZE(views); i++) {
			if(i % 6 == 0) {
				if(i > 0)
					vid_printf("\n");
				vid_printf("\t");
			}
			vid_printf("%s ",views[i].name);
		}
		vid_printf("\n");
		return 0;
	}

	for(i = 0; i < ARRAY_SIZE(views); i++) {
		if(strcmp(views[i].name,argv[1]) == 0) {
			/* create lines and redirect prints */
			if((res = lines_create(&lines)) < 0)
				return res;
			vid_setPrintFunc(view_printc);
			err = 0;
			views[i].func(argc,argv);
			if(err < 0) {
				vid_unsetPrintFunc();
				lines_destroy(&lines);
				return err;
			}
			lines_end(&lines);
			vid_unsetPrintFunc();
			break;
		}
	}
	if(i == ARRAY_SIZE(views))
		return ERR_INVALID_ARGS;

	/* view the lines */
	vid_backup(backup.screen,&backup.row,&backup.col);
	cons_viewLines(&lines);
	lines_destroy(&lines);
	vid_restore(backup.screen,backup.row,backup.col);
	return 0;
}

static void view_printc(char c) {
	if(err < 0)
		return;
	if(c == '\n')
		err = lines_newline(&lines);
	else
		lines_append(&lines,c);
}

static void view_proc(s32 argc,char **argv) {
	sProc *p = view_getProc(argc,argv);
	if(p != NULL)
		proc_dbg_print(p);
}
static void view_procs(void) {
	proc_dbg_printAll();
}
static void view_sched(void) {
	sched_dbg_print();
}
static void view_signals(void) {
	sig_dbg_print();
}
static void view_thread(s32 argc,char **argv) {
	sThread *t = view_getThread(argc,argv);
	if(t != NULL)
		thread_dbg_print(t);
}
static void view_threads(void) {
	thread_dbg_printAll();
}
static void view_vfstree(void) {
	vfsn_dbg_printTree();
}
static void view_gft(void) {
	vfs_dbg_printGFT();
}
static void view_cow(void) {
	cow_dbg_print();
}
static void view_kheap(void) {
	kheap_dbg_print();
}
static void view_pdirall(s32 argc,char **argv) {
	sProc *p = view_getProc(argc,argv);
	if(p != NULL)
		paging_dbg_printPDir(p->pagedir,PD_PART_ALL);
}
static void view_pdiruser(s32 argc,char **argv) {
	sProc *p = view_getProc(argc,argv);
	if(p != NULL)
		paging_dbg_printPDir(p->pagedir,PD_PART_USER);
}
static void view_pdirkernel(s32 argc,char **argv) {
	sProc *p = view_getProc(argc,argv);
	if(p != NULL)
		paging_dbg_printPDir(p->pagedir,PD_PART_KERNEL);
}
static void view_regions(s32 argc,char **argv) {
	if(argc < 3)
		proc_dbg_printAllRegions();
	else {
		sProc *p = view_getProc(argc,argv);
		if(p != NULL)
			vmm_dbg_print(p);
	}
}
static void view_pmem(void) {
	mm_dbg_printFreeFrames(MM_DEF | MM_CONT);
}
static void view_pmemcont(void) {
	mm_dbg_printFreeFrames(MM_CONT);
}
static void view_pmemstack(void) {
	mm_dbg_printFreeFrames(MM_DEF);
}
static void view_shm(void) {
	shm_dbg_print();
}
static void view_cpu(void) {
	cpu_dbg_print();
}
static void view_gdt(void) {
	gdt_dbg_print();
}
static void view_timer(void) {
	timer_dbg_print();
}
static void view_kevents(void) {
	kev_dbg_print();
}
static void view_multiboot(void) {
	mboot_dbg_print();
}

static sProc *view_getProc(s32 argc,char **argv) {
	sProc *p;
	if(argc > 2)
		p = proc_getByPid(atoi(argv[2]));
	else
		p = proc_getRunning();
	if(p == NULL)
		vid_printf("Unable to find process '%s'\n",argv[2]);
	return p;
}

static sThread *view_getThread(s32 argc,char **argv) {
	sThread *t;
	if(argc > 2)
		t = thread_getById(atoi(argv[2]));
	else
		t = thread_getRunning();
	if(t == NULL)
		vid_printf("Unable to find thread '%s'\n",argv[2]);
	return t;
}
