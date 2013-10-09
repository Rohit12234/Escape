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
#include <usergroup/user.h>
#include <usergroup/group.h>
#include <usergroup/passwd.h>
#include <esc/driver/vterm.h>
#include <esc/messages.h>
#include <esc/thread.h>
#include <esc/proc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SKIP_LOGIN			1
#define SHELL_PATH			"/bin/shell"
#define MAX_VTERM_NAME_LEN	10

static sUser *getUser(const char *user,const char *pw);

static sPasswd *pwList = NULL;
static sGroup *groupList;
static sUser *userList = NULL;

int main(int argc,char **argv) {
	const char *shargs[] = {NULL,NULL,NULL};
	char un[MAX_USERNAME_LEN + 1];
	char pw[MAX_PW_LEN + 1];
	sUser *u;
	sGroup *gvt;
	gid_t *groups;
	size_t groupCount,usercount,pwcount;
	int vterm;
	int fd;

	if(argc != 2)
		error("Usage: %s <vterm>",argv[0]);

	/* parse vterm-number from "/dev/vtermX" */
	vterm = atoi(argv[1] + 10);
	/* note: we do always pass IO_MSGS to open because the user might want to request the console
	 * size or use isatty() or something. */
	if((fd = open(argv[1],IO_READ | IO_MSGS)) != STDIN_FILENO)
		error("Unable to open '%s' for STDIN: Got fd %d",argv[1],fd);

	/* open stdout */
	if((fd = open(argv[1],IO_WRITE | IO_MSGS)) != STDOUT_FILENO)
		error("Unable to open '%s' for STDOUT: Got fd %d",argv[1],fd);

	/* dup stdout to stderr */
	if((fd = dup(fd)) != STDERR_FILENO)
		error("Unable to duplicate STDOUT to STDERR: Got fd %d",fd);

	printf("\n\n");
	printf("\033[co;9]Welcome to Escape v%s, %s!\033[co]\n\n",ESCAPE_VERSION,argv[1]);
	printf("Please login to get a shell.\n");
	printf("Hint: use hrniels/test, jon/doe or root/root ;)\n\n");

	while(1) {
#if SKIP_LOGIN
		strcpy(un,"root");
		strcpy(pw,"root");
#else
		printf("Username: ");
		fgetl(un,sizeof(un),stdin);
		vterm_setEcho(STDOUT_FILENO,false);
		printf("Password: ");
		fgetl(pw,sizeof(pw),stdin);
		vterm_setEcho(STDOUT_FILENO,true);
		putchar('\n');
#endif

		/* re-read users */
		user_free(userList);
		userList = user_parseFromFile(USERS_PATH,&usercount);
		if(!userList)
			error("Unable to parse users from '%s'",USERS_PATH);

		pw_free(pwList);
		pwList = pw_parseFromFile(PASSWD_PATH,&pwcount);
		if(!pwList)
			error("Unable to parse passwords from '%s'",PASSWD_PATH);

		u = getUser(un,pw);
		if(u != NULL)
			break;

		printf("Sorry, invalid username or password. Try again!\n");
		fflush(stdout);
		sleep(1000);
	}
	fflush(stdout);

	/* read in groups */
	groupList = group_parseFromFile(GROUPS_PATH,&usercount);
	if(!groupList)
		error("Unable to parse groups from '%s'",GROUPS_PATH);

	/* set user- and group-id */
	if(setgid(u->gid) < 0)
		error("Unable to set gid");
	if(setuid(u->uid) < 0)
		error("Unable to set uid");
	/* determine groups and set them */
	groups = group_collectGroupsFor(groupList,u->uid,1,&groupCount);
	if(!groups)
		error("Unable to collect group-ids");
	gvt = group_getByName(groupList,argv[1]);
	/* add the process to the corresponding vterm-group */
	if(gvt)
		groups[groupCount++] = gvt->gid;
	if(setgroups(groupCount,groups) < 0)
		error("Unable to set groups");

	/* cd to home-dir */
	if(isdir(u->home))
		setenv("CWD",u->home);
	else
		setenv("CWD","/");
	setenv("HOME",getenv("CWD"));
	setenv("USER",u->name);

	/* exchange with shell */
	shargs[0] = argv[0];
	shargs[1] = argv[1];
	exec(SHELL_PATH,shargs);

	/* not reached */
	return EXIT_SUCCESS;
}

static sUser *getUser(const char *name,const char *pw) {
	sUser *u = userList;
	while(u != NULL) {
		if(strcmp(u->name,name) == 0) {
			sPasswd *p = pw_getById(pwList,u->uid);
			if(p && strcmp(p->pw,pw) == 0)
				return u;
			return NULL;
		}
		u = u->next;
	}
	return NULL;
}
