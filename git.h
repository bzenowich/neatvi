/* git integration */

struct git_info {
	char branch[128];	/* current git branch */
	char stats[64];		/* diff stats: "+5 -1" format */
	int valid;		/* 1 if this file is in a git repo */
};

void git_update(struct git_info *info, char *path);
