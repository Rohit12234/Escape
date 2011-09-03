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
#include <esc/proc.h>
#include <esc/thread.h>
#include <esc/driver.h>
#include <esc/messages.h>
#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include "initerror.h"
#include "process/processmanager.h"

#define SHUTDOWN_TIMEOUT		3000
#define STATE_RUN				0
#define STATE_REBOOT			1
#define STATE_SHUTDOWN			2

static void sigAlarm(int sig);
static int driverThread(void *arg);

static ProcessManager pm;
static bool shuttingDown = false;
static bool timeout = false;
static int state = STATE_RUN;

int main(void) {
	if(getpid() != 0) {
		cerr << "It's not good to start init twice ;)" << endl;
		return EXIT_FAILURE;
	}

	if(startThread(driverThread,NULL) < 0) {
		cerr << "Unable to start driver-thread" << endl;
		return EXIT_FAILURE;
	}

	try {
		pm.start();
	}
	catch(const init_error& e) {
		cerr << "Unable to init system: " << e.what() << endl;
		return EXIT_FAILURE;
	}

	// loop and wait forever
	while(1) {
		sExitState st;
		waitChild(&st);
		if(state != STATE_RUN)
			pm.died(st.pid);
		else
			pm.restart(st.pid);
	}
	return EXIT_SUCCESS;
}

static void sigAlarm(int sig) {
	UNUSED(sig);
	timeout = true;
}

static int driverThread(void *arg) {
	int drv = regDriver("init",0);
	if(drv < 0)
		error("Unable to register device 'init'");
	if(setSigHandler(SIG_ALARM,sigAlarm) < 0)
		error("Unable to set alarm-handler");

	while(!timeout) {
		msgid_t mid;
		sMsg msg;
		int fd = getWork(&drv,1,NULL,&mid,&msg,sizeof(msg),0);
		if(fd < 0) {
			if(fd != ERR_INTERRUPTED)
				printe("[INIT] Unable to get work");
		}
		else {
			switch(mid) {
				case MSG_INIT_REBOOT:
					if(state == STATE_RUN) {
						state = STATE_REBOOT;
						if(alarm(SHUTDOWN_TIMEOUT) < 0)
							printe("[INIT] Unable to set alarm");
						pm.shutdown();
					}
					break;

				case MSG_INIT_SHUTDOWN:
					if(state == STATE_RUN) {
						state = STATE_SHUTDOWN;
						if(alarm(SHUTDOWN_TIMEOUT) < 0)
							printe("[INIT] Unable to set alarm");
						pm.shutdown();
					}
					break;

				case MSG_INIT_IAMALIVE:
					if(state != STATE_RUN)
						pm.setAlive((pid_t)msg.args.arg1);
					break;

				default:
					msg.args.arg1 = ERR_UNSUPPORTED_OP;
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.args));
					break;
			}
			close(fd);
		}
	}

	if(state == STATE_REBOOT)
		pm.finalize(ProcessManager::TASK_REBOOT);
	else
		pm.finalize(ProcessManager::TASK_SHUTDOWN);
	close(drv);
	return 0;
}
