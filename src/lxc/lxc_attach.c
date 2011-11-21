/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2010
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "commands.h"
#include "arguments.h"
#include "namespace.h"
#include "caps.h"
#include "log.h"
#include "conf.h"
#include "confile.h"

lxc_log_define(lxc_attach_ui, lxc);

static struct lxc_list defines;

static int my_parser(struct lxc_arguments* args, int c, char* arg)
{
	switch (c) {
	case 'f': args->rcfile = arg; break;
	}
	return 0;
}

static const struct option my_longopts[] = {
	{"rcfile", required_argument, 0, 'f'},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname = "lxc-attach",
	.help     = "\
--name=NAME\n\
\n\
Execute the specified command - enter the container NAME\n\
\n\
Options :\n\
  -n, --name=NAME   NAME for name of the container\n\
  -f, --rcfile=FILE Load configuration file FILE\n",
	.options  = my_longopts,
	.parser   = my_parser,
	.checker  = NULL,
};

int main(int argc, char *argv[], char *envp[])
{
	int ret;
	pid_t pid;
	struct passwd *passwd;
	uid_t uid;
	char *curdir;
	char *rcfile;
	struct lxc_conf *conf;

	lxc_list_init(&defines);

	ret = lxc_caps_init();
	if (ret)
		return ret;

	ret = lxc_arguments_parse(&my_args, argc, argv);
	if (ret)
		return ret;

	ret = lxc_log_init(my_args.log_file, my_args.log_priority,
			   my_args.progname, my_args.quiet);
	if (ret)
		return ret;

	/* rcfile is specified in the cli option */
	if (my_args.rcfile)
		rcfile = (char *)my_args.rcfile;
	else {
		int rc;

		rc = asprintf(&rcfile, LXCPATH "/%s/config", my_args.name);
		if (rc == -1) {
			SYSERROR("failed to allocate memory");
			return -1;
		}

		/* container configuration does not exist */
		if (access(rcfile, F_OK)) {
			free(rcfile);
			rcfile = NULL;
		}
	}

	conf = lxc_conf_init();
	if (!conf) {
		ERROR("failed to initialize configuration");
		return -1;
	}

	if (rcfile && lxc_config_read(rcfile, conf)) {
		ERROR("failed to read configuration file");
		return -1;
	}

	if (lxc_config_define_load(&defines, conf))
		return -1;
	pid = get_init_pid(my_args.name);
	if (pid < 0) {
		ERROR("failed to get the init pid");
		return -1;
	}

	curdir = get_current_dir_name();

	ret = lxc_attach(pid, my_args.name);
	if (ret < 0) {
		ERROR("failed to enter the namespace");
		return -1;
	}

	if (curdir && chdir(curdir))
		WARN("could not change directory to '%s'", curdir);

	free(curdir);

	if (setup_caps(&conf->caps)) {
		ERROR("failed to drop capabilities");
		return -1;
	}

	pid = fork();

	if (pid < 0) {
		SYSERROR("failed to fork");
		return -1;
	}

	if (pid) {
		int status;

	again:
		if (waitpid(pid, &status, 0) < 0) {
			if (errno == EINTR)
				goto again;
			SYSERROR("failed to wait '%d'", pid);
			return -1;
		}

		if (WIFEXITED(status))
			return WEXITSTATUS(status);

		return -1;
	}

	if (!pid) {

		if (my_args.argc) {
			execve(my_args.argv[0], my_args.argv, envp);
			SYSERROR("failed to exec '%s'", my_args.argv[0]);
			return -1;
		}

		uid = getuid();

		passwd = getpwuid(uid);
		if (!passwd) {
			SYSERROR("failed to get passwd "		\
				 "entry for uid '%d'", uid);
			return -1;
		}

		{
			char *const args[] = {
				passwd->pw_shell,
				NULL,
			};

			execve(args[0], args, envp);
			SYSERROR("failed to exec '%s'", args[0]);
			return -1;
		}

	}

	return 0;
}
