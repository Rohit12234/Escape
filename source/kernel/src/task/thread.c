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

#include <sys/common.h>
#include <sys/task/thread.h>
#include <sys/task/proc.h>
#include <sys/task/signals.h>
#include <sys/task/event.h>
#include <sys/task/timer.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/info.h>
#include <sys/vfs/node.h>
#include <sys/vfs/real.h>
#include <sys/vfs/request.h>
#include <sys/mem/cache.h>
#include <sys/mem/paging.h>
#include <sys/mem/pmem.h>
#include <sys/mem/swap.h>
#include <sys/mem/vmm.h>
#include <sys/mem/sllnodes.h>
#include <sys/task/sched.h>
#include <sys/task/lock.h>
#include <sys/task/smp.h>
#include <sys/klock.h>
#include <sys/util.h>
#include <sys/video.h>
#include <assert.h>
#include <string.h>
#include <errors.h>

static sThread *thread_createInitial(sProc *p,eThreadState state);
static tid_t thread_getFreeTid(void);
static bool thread_add(sThread *t);
static void thread_remove(sThread *t);

/* our threads */
static sSLList *threads;
static sThread *tidToThread[MAX_THREAD_COUNT];
static sSLList *idleThreads;
static tid_t nextTid = 0;

sThread *thread_init(sProc *p) {
	sThread *curThread;

	threads = sll_create();
	if(!threads)
		util_panic("Unable to create thread-list");
	idleThreads = sll_create();
	if(!idleThreads)
		util_panic("Unable to create idle-thread-list");

	/* create thread for init */
	curThread = thread_createInitial(p,ST_RUNNING);
	thread_setRunning(curThread);
	return curThread;
}

static sThread *thread_createInitial(sProc *p,eThreadState state) {
	size_t i;
	sThread *t = (sThread*)cache_alloc(sizeof(sThread));
	if(t == NULL)
		util_panic("Unable to allocate mem for initial thread");

	*(uint8_t*)&t->flags = 0;
	*(tid_t*)&t->tid = nextTid++;
	*(sProc**)&t->proc = p;

	t->state = state;
	t->events = 0;
	t->waits = NULL;
	t->ignoreSignals = 0;
	t->signal = SIG_COUNT;
	t->intrptLevel = 0;
	t->cpu = -1;
	t->stats.ucycleCount.val64 = 0;
	t->stats.ucycleStart = 0;
	t->stats.kcycleCount.val64 = 0;
	t->stats.kcycleStart = 0;
	t->stats.schedCount = 0;
	t->stats.syscalls = 0;
	sll_init(&t->termHeapAllocs,slln_allocNode,slln_freeNode);
	sll_init(&t->termCallbacks,slln_allocNode,slln_freeNode);
	sll_init(&t->termLocks,slln_allocNode,slln_freeNode);
	for(i = 0; i < STACK_REG_COUNT; i++)
		t->stackRegions[i] = -1;
	t->tlsRegion = -1;
	if(thread_initArch(t) < 0)
		util_panic("Unable to init the arch-specific attributes of initial thread");

	/* create list */
	if(!thread_add(t))
		util_panic("Unable to put initial thread into the thread-list");

	/* insert in VFS; thread needs to be inserted for it */
	if(!vfs_createThread(t->tid))
		util_panic("Unable to put first thread in vfs");

	return t;
}

bool thread_setSignal(sThread *t,sig_t sig) {
	assert(t->signal == SIG_COUNT);
	if(!t->ignoreSignals) {
		t->signal = sig;
		return true;
	}
	return false;
}

sig_t thread_getSignal(const sThread *t) {
	return t->signal;
}

void thread_unsetSignal(sThread *t) {
	t->signal = SIG_COUNT;
}

sIntrptStackFrame *thread_getIntrptStack(const sThread *t) {
	assert(t->intrptLevel > 0);
	return t->intrptLevels[t->intrptLevel - 1];
}

void thread_pushIntrptLevel(sThread *t,sIntrptStackFrame *stack) {
	t->intrptLevels[t->intrptLevel++] = stack;
}

void thread_popIntrptLevel(sThread *t) {
	assert(t->intrptLevel > 0);
	t->intrptLevel--;
}

size_t thread_getIntrptLevel(const sThread *t) {
	assert(t->intrptLevel > 0);
	return t->intrptLevel - 1;
}

size_t thread_getCount(void) {
	return sll_length(threads);
}

sThread *thread_getById(tid_t tid) {
	if(tid >= ARRAY_SIZE(tidToThread))
		return NULL;
	return tidToThread[tid];
}

void thread_pushIdle(sThread *t) {
	sll_append(idleThreads,t);
}

sThread *thread_popIdle(void) {
	return sll_removeFirst(idleThreads);
}

void thread_switch(void) {
	thread_switchTo(sched_perform()->tid);
}

void thread_switchNoSigs(void) {
	sThread *t = thread_getRunning();
	/* remember that the current thread wants to ignore signals */
	t->ignoreSignals = 1;
	thread_switch();
	t->ignoreSignals = 0;
}

void thread_block(sThread *t) {
	assert(t != NULL);
	sched_setBlocked(t);
}

void thread_unblock(sThread *t) {
	assert(t != NULL && t != thread_getRunning());
	sched_setReady(t);
}

void thread_suspend(sThread *t) {
	assert(t != NULL);
	sched_setSuspended(t,true);
}

void thread_unsuspend(sThread *t) {
	assert(t != NULL);
	sched_setSuspended(t,false);
}

bool thread_getStackRange(const sThread *t,uintptr_t *start,uintptr_t *end,size_t stackNo) {
	if(t->stackRegions[stackNo] >= 0) {
		vmm_getRegRange(t->proc,t->stackRegions[stackNo],start,end);
		return true;
	}
	return false;
}

bool thread_getTLSRange(const sThread *t,uintptr_t *start,uintptr_t *end) {
	if(t->tlsRegion >= 0) {
		vmm_getRegRange(t->proc,t->tlsRegion,start,end);
		return true;
	}
	return false;
}

vmreg_t thread_getTLSRegion(const sThread *t) {
	return t->tlsRegion;
}

void thread_setTLSRegion(sThread *t,vmreg_t rno) {
	t->tlsRegion = rno;
}

bool thread_hasStackRegion(const sThread *t,vmreg_t regNo) {
	size_t i;
	for(i = 0; i < STACK_REG_COUNT; i++) {
		if(t->stackRegions[i] == regNo)
			return true;
	}
	return false;
}

void thread_removeRegions(sThread *t,bool remStack) {
	t->tlsRegion = -1;
	if(remStack) {
		size_t i;
		for(i = 0; i < STACK_REG_COUNT; i++)
			t->stackRegions[i] = -1;
	}
	/* remove all signal-handler since we've removed the code to handle signals */
	sig_removeHandlerFor(t->tid);
}

int thread_extendStack(uintptr_t address) {
	sThread *t = thread_getRunning();
	size_t i;
	int res = 0;
	for(i = 0; i < STACK_REG_COUNT; i++) {
		/* if it does not yet exist, report an error */
		if(t->stackRegions[i] < 0)
			return ERR_NOT_ENOUGH_MEM;

		res = vmm_growStackTo(t,t->stackRegions[i],address);
		if(res >= 0)
			return res;
	}
	return res;
}

void thread_addLock(klock_t *lock) {
	sThread *t = thread_getRunning();
	sll_append(&t->termLocks,lock);
}

void thread_remLock(klock_t *lock) {
	sThread *t = thread_getRunning();
	sll_removeFirstWith(&t->termLocks,lock);
}

void thread_addHeapAlloc(void *ptr) {
	sThread *t = thread_getRunning();
	sll_append(&t->termHeapAllocs,ptr);
}

void thread_remHeapAlloc(void *ptr) {
	sThread *t = thread_getRunning();
	sll_removeFirstWith(&t->termHeapAllocs,ptr);
}

void thread_addCallback(fTermCallback cb) {
	sThread *t = thread_getRunning();
	sll_append(&t->termCallbacks,cb);
}

void thread_remCallback(fTermCallback cb) {
	sThread *t = thread_getRunning();
	sll_removeFirstWith(&t->termCallbacks,cb);
}

int thread_clone(const sThread *src,sThread **dst,sProc *p,uint8_t flags,frameno_t stackFrame,
		bool cloneProc) {
	int err = ERR_NOT_ENOUGH_MEM;
	sThread *t = (sThread*)cache_alloc(sizeof(sThread));
	if(t == NULL)
		return ERR_NOT_ENOUGH_MEM;

	*(tid_t*)&t->tid = thread_getFreeTid();
	if(t->tid == INVALID_TID) {
		err = ERR_NO_FREE_THREADS;
		goto errThread;
	}
	*(uint8_t*)&t->flags = flags;
	*(sProc**)&t->proc = p;

	t->state = ST_RUNNING;
	t->events = 0;
	t->waits = NULL;
	t->ignoreSignals = 0;
	t->signal = SIG_COUNT;
	t->cpu = -1;
	t->stats.kcycleCount.val64 = 0;
	t->stats.kcycleStart = 0;
	t->stats.ucycleCount.val64 = 0;
	t->stats.ucycleStart = 0;
	t->stats.schedCount = 0;
	t->stats.syscalls = 0;
	t->intrptLevel = src->intrptLevel;
	memcpy(t->intrptLevels,src->intrptLevels,sizeof(sIntrptStackFrame*) * MAX_INTRPT_LEVELS);
	sll_init(&t->termHeapAllocs,slln_allocNode,slln_freeNode);
	sll_init(&t->termCallbacks,slln_allocNode,slln_freeNode);
	sll_init(&t->termLocks,slln_allocNode,slln_freeNode);
	if(cloneProc) {
		size_t i;
		t->kstackFrame = stackFrame;
		for(i = 0; i < STACK_REG_COUNT; i++)
			t->stackRegions[i] = src->stackRegions[i];
		t->tlsRegion = src->tlsRegion;
	}
	else {
		/* add kernel-stack */
		t->kstackFrame = pmem_allocate();
		/* add a new tls-region, if its present in the src-thread */
		t->tlsRegion = -1;
		if(src->tlsRegion >= 0) {
			uintptr_t tlsStart,tlsEnd;
			vmm_getRegRange(src->proc,src->tlsRegion,&tlsStart,&tlsEnd);
			t->tlsRegion = vmm_add(p,NULL,0,tlsEnd - tlsStart,tlsEnd - tlsStart,REG_TLS);
			if(t->tlsRegion < 0)
				goto errStack;
		}
	}

	/* clone architecture-specific stuff */
	if((err = thread_cloneArch(src,t,cloneProc)) < 0)
		goto errClone;

	/* insert into thread-list */
	if(!thread_add(t))
		goto errArch;

	/* append to idle-list if its an idle-thread */
	if(flags & T_IDLE)
		thread_pushIdle(t);

	/* clone signal-handler (here because the thread needs to be in the map first) */
	if(cloneProc)
		sig_cloneHandler(src->tid,t->tid);

	/* insert in VFS; thread needs to be inserted for it */
	if(!vfs_createThread(t->tid))
		goto errAppendIdle;

	*dst = t;
	return 0;

errAppendIdle:
	sig_removeHandlerFor(t->tid);
	if(flags & T_IDLE)
		sll_removeFirstWith(idleThreads,t);
errAppend:
	thread_remove(t);
errArch:
	thread_freeArch(t);
errClone:
	if(t->tlsRegion >= 0)
		vmm_remove(p,t->tlsRegion);
errStack:
	if(!cloneProc)
		pmem_free(t->kstackFrame);
errThread:
	cache_free(t);
	return err;
}

bool thread_kill(sThread *t) {
	sSLNode *n;
	if(t->tid == INIT_TID)
		util_panic("Can't kill init-thread!");
	/* we can't destroy the current thread */
	if(t == thread_getRunning()) {
		/* remove from event-system */
		ev_removeThread(t);
		/* remove from scheduler and ensure that he don't picks us again */
		sched_removeThread(t);
		/* remove from timer, too, so that we don't get waked up again */
		timer_removeThread(t->tid);
		t->state = ST_ZOMBIE;
		return false;
	}

	/* remove tls */
	if(t->tlsRegion >= 0) {
		vmm_remove(t->proc,t->tlsRegion);
		t->tlsRegion = -1;
	}
	/* free kernel-stack */
	pmem_free(t->kstackFrame);

	/* release resources */
	for(n = sll_begin(&t->termLocks); n != NULL; n = n->next)
		klock_release((klock_t*)n->data);
	sll_clear(&t->termLocks);
	for(n = sll_begin(&t->termHeapAllocs); n != NULL; n = n->next)
		cache_free(n->data);
	sll_clear(&t->termHeapAllocs);
	for(n = sll_begin(&t->termCallbacks); n != NULL; n = n->next) {
		fTermCallback cb = (fTermCallback)n->data;
		cb();
	}
	sll_clear(&t->termCallbacks);

	/* remove from all modules we may be announced */
	sig_removeHandlerFor(t->tid);
	ev_removeThread(t);
	sched_removeThread(t);
	timer_removeThread(t->tid);
	thread_freeArch(t);
	vfs_removeThread(t->tid);
	vfs_req_freeAllOf(t);

	/* notify the process about it */
	sig_addSignalFor(t->proc->pid,SIG_THREAD_DIED);
	ev_wakeup(EVI_THREAD_DIED,(evobj_t)t->proc);

	/* finally, destroy thread */
	thread_remove(t);
	cache_free(t);
	return true;
}

void thread_printAll(void) {
	sSLNode *n;
	vid_printf("Threads:\n");
	for(n = sll_begin(threads); n != NULL; n = n->next) {
		sThread *t = (sThread*)n->data;
		thread_print(t);
	}
}

void thread_print(const sThread *t) {
	size_t i;
	sFuncCall *calls;
	static const char *states[] = {
		"UNUSED","RUNNING","READY","BLOCKED","ZOMBIE","BLOCKEDSWAP","READYSWAP"
	};
	vid_printf("\tThread %d: (process %d:%s)\n",t->tid,t->proc->pid,t->proc->command);
	vid_printf("\t\tFlags=%#x\n",t->flags);
	vid_printf("\t\tState=%s\n",states[t->state]);
	vid_printf("\t\tEvents=");
	ev_printEvMask(t);
	vid_printf("\n");
	vid_printf("\t\tLastCPU=%d\n",t->cpu);
	vid_printf("\t\tKstackFrame=%#Px\n",t->kstackFrame);
	vid_printf("\t\tTlsRegion=%d, ",t->tlsRegion);
	for(i = 0; i < STACK_REG_COUNT; i++) {
		vid_printf("stackRegion%zu=%d",i,t->stackRegions[i]);
		if(i < STACK_REG_COUNT - 1)
			vid_printf(", ");
	}
	vid_printf("\n");
	vid_printf("\t\tUCycleCount = %#016Lx\n",t->stats.ucycleCount.val64);
	vid_printf("\t\tKCycleCount = %#016Lx\n",t->stats.kcycleCount.val64);
	vid_printf("\t\tKernel-trace:\n");
	calls = util_getKernelStackTraceOf(t);
	while(calls->addr != 0) {
		vid_printf("\t\t\t%p -> %p (%s)\n",(calls + 1)->addr,calls->funcAddr,calls->funcName);
		calls++;
	}
	calls = util_getUserStackTraceOf(t);
	if(calls) {
		vid_printf("\t\tUser-trace:\n");
		while(calls->addr != 0) {
			vid_printf("\t\t\t%p -> %p (%s)\n",
					(calls + 1)->addr,calls->funcAddr,calls->funcName);
			calls++;
		}
	}
}

static tid_t thread_getFreeTid(void) {
	size_t count = 0;
	while(count < MAX_THREAD_COUNT) {
		if(nextTid >= MAX_THREAD_COUNT)
			nextTid = 0;
		if(tidToThread[nextTid++] == NULL)
			return nextTid - 1;
		count++;
	}
	return INVALID_TID;
}

static bool thread_add(sThread *t) {
	if(!sll_append(threads,t))
		return false;
	tidToThread[t->tid] = t;
	return true;
}

static void thread_remove(sThread *t) {
	sll_removeFirstWith(threads,t);
	tidToThread[t->tid] = NULL;
}
