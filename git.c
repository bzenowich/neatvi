/*
 * Git integration for neatvi
 *
 * Copyright (C) 2015-2025 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "git.h"

/* update git info for current file */
void git_update(struct git_info *info, char *path)
{
	FILE *fp;
	char cmd[512];
	char dir[512];
	char abspath[4096];
	char *fullpath;
	char *slash;
	char *filename;

	info->branch[0] = '\0';
	info->stats[0] = '\0';
	info->valid = 0;

	if (!path || !path[0])
		return;

	/* resolve full path - if file doesn't exist yet, get dir of current directory */
	if (realpath(path, abspath)) {
		fullpath = abspath;
	} else if (path[0] == '/') {
		/* absolute path to non-existent file - use parent dir */
		fullpath = path;
	} else {
		/* relative path to non-existent file - resolve current dir + path */
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd))) {
			snprintf(abspath, sizeof(abspath), "%s/%s", cwd, path);
			fullpath = abspath;
		} else {
			fullpath = path;
		}
	}

	/* find directory of current file */
	slash = strrchr(fullpath, '/');
	if (slash) {
		int len = slash - fullpath;
		if (len >= sizeof(dir))
			len = sizeof(dir) - 1;
		memcpy(dir, fullpath, len);
		dir[len] = '\0';
		filename = slash + 1;
	} else {
		strcpy(dir, ".");
		filename = fullpath;
	}

	/* get git branch */
	snprintf(cmd, sizeof(cmd), "cd \"%s\" 2>/dev/null && git rev-parse --abbrev-ref HEAD 2>/dev/null", dir);
	fp = popen(cmd, "r");
	if (fp) {
		if (fgets(info->branch, sizeof(info->branch), fp)) {
			char *nl = strchr(info->branch, '\n');
			if (nl)
				*nl = '\0';
			info->valid = 1;
		}
		pclose(fp);
	}

	/* get git diff stats for this file */
	if (info->valid) {
		int added = 0, deleted = 0;
		snprintf(cmd, sizeof(cmd), "cd \"%s\" 2>/dev/null && git diff --numstat \"%s\" 2>/dev/null",
			dir, filename);
		fp = popen(cmd, "r");
		if (fp) {
			if (fscanf(fp, "%d %d", &added, &deleted) == 2) {
				snprintf(info->stats, sizeof(info->stats), "+%d -%d", added, deleted);
			} else {
				/* no changes */
				strcpy(info->stats, "+0 -0");
			}
			pclose(fp);
		}
	}
}
