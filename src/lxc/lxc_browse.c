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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <err.h>
#undef _GNU_SOURCE

#include "lxc.h"
#include "config.h"

static int _unselect_current_and_parent(const struct dirent* d)
{
    return (strcmp(d->d_name, ".") && strcmp(d->d_name, ".."));
}

static int _isdir(const char *path)
{
    struct stat sb;

    if (stat(path, &sb) == -1) {
        warn("_isdir, can't stat path: %s", path);
        return 0;
    }

    return (sb.st_mode & S_IFDIR);
}

static char *_join_path(char *dest, size_t size,
		const char *left, const char *right)
{
    if (*right != '/') {
		if (strlen(left) + 1 + strlen(right) > size) {
			warn("_join_path, path overflow: %s + / + %s", left, right);
			return 0;
		}
        strcpy(dest, left);
        strcat(dest, "/");
        strcat(dest, right);
    } else {
		if (strlen(left) + strlen(right) > size) {
			warn("_join_path, path overflow: %s", right);
			return 0;
		}
        strcpy(dest, right);
    }
    return dest;
}

int lxc_browse(int (*cb)(void*, const char*), void *ctx)
{
	int             i;
	struct dirent   **c_vec;
	char            buf[PATH_MAX];
	int             count;

	count = scandir(LXCPATH, &c_vec, &_unselect_current_and_parent, &versionsort);
	if (count == -1)
		return (-1);

	for (i = 0; i < count; ++i)
	{
		if (_isdir(
					_join_path(buf, sizeof(buf), LXCPATH, c_vec[i]->d_name))
				)
		{
			if (cb(ctx, c_vec[i]->d_name)) {
				errno = EINTR;
				goto abort_by_cb;
			}
		}
		free(c_vec[i]);
	}
	free(c_vec);
	return count;

abort_by_cb:
	for (; i < count; ++i)
		free(c_vec[i]);
	free(c_vec);
	return -1;
}
