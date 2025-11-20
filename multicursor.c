/* multi-cursor editing support */
#include <stdlib.h>
#include <string.h>
#include "vi.h"
#include "multicursor.h"

struct mc {
	struct mc_cursor *cursors;	/* array of cursors */
	int cursor_n;			/* number of active cursors */
	int cursor_sz;			/* allocated size of cursors array */
};

struct mc *mc_make(void)
{
	struct mc *mc = malloc(sizeof(*mc));
	memset(mc, 0, sizeof(*mc));
	return mc;
}

void mc_free(struct mc *mc)
{
	if (mc) {
		free(mc->cursors);
		free(mc);
	}
}

void mc_add(struct mc *mc, int row, int off)
{
	int i;
	/* check if cursor already exists at this position */
	for (i = 0; i < mc->cursor_n; i++) {
		if (mc->cursors[i].row == row && mc->cursors[i].off == off)
			return;	/* cursor already exists */
	}
	/* expand array if needed */
	if (mc->cursor_n >= mc->cursor_sz) {
		int nsz = mc->cursor_sz ? mc->cursor_sz * 2 : 8;
		struct mc_cursor *ncursors = malloc(nsz * sizeof(struct mc_cursor));
		if (mc->cursors) {
			memcpy(ncursors, mc->cursors, mc->cursor_n * sizeof(struct mc_cursor));
			free(mc->cursors);
		}
		mc->cursors = ncursors;
		mc->cursor_sz = nsz;
	}
	/* add new cursor */
	mc->cursors[mc->cursor_n].row = row;
	mc->cursors[mc->cursor_n].off = off;
	mc->cursor_n++;
}

void mc_clear(struct mc *mc)
{
	mc->cursor_n = 0;
}

int mc_count(struct mc *mc)
{
	return mc->cursor_n;
}

int mc_get(struct mc *mc, int i, int *row, int *off)
{
	if (i < 0 || i >= mc->cursor_n)
		return 1;
	*row = mc->cursors[i].row;
	*off = mc->cursors[i].off;
	return 0;
}

void mc_set(struct mc *mc, int i, int row, int off)
{
	if (i >= 0 && i < mc->cursor_n) {
		mc->cursors[i].row = row;
		mc->cursors[i].off = off;
	}
}

int mc_active(struct mc *mc)
{
	return mc->cursor_n > 0;
}

void mc_remove(struct mc *mc, int i)
{
	if (i >= 0 && i < mc->cursor_n) {
		/* shift remaining cursors down */
		memmove(&mc->cursors[i], &mc->cursors[i + 1],
			(mc->cursor_n - i - 1) * sizeof(struct mc_cursor));
		mc->cursor_n--;
	}
}

/* update cursor positions after text modification
 * this function adjusts cursor positions similar to how lbuf_replace()
 * adjusts marks in lbuf.c lines 181-188
 */
void mc_adjust(struct mc *mc, int pos, int n_del, int n_ins, int off, int off_delta)
{
	int i;
	for (i = 0; i < mc->cursor_n; i++) {
		struct mc_cursor *c = &mc->cursors[i];

		/* if cursor is on a deleted line, remove it */
		if (c->row >= pos && c->row < pos + n_del && n_del > 0 && n_ins == 0) {
			mc_remove(mc, i);
			i--;	/* adjust index since we removed an element */
			continue;
		}

		/* adjust cursor row if it's after the modification */
		if (c->row >= pos + n_del) {
			c->row += n_ins - n_del;
		}

		/* adjust offset if cursor is on the modified line */
		if (c->row == pos && off_delta != 0) {
			if (c->off >= off)
				c->off += off_delta;
			/* ensure offset doesn't go negative */
			if (c->off < 0)
				c->off = 0;
		}
	}
}
