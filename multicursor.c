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

/* build a display line with visual cursor markers */
char *mc_build_display_line(struct mc *mc, char *line, int row)
{
	int i, n = mc->cursor_n;
	int has_cursor = 0;
	struct sbuf *marked;
	int last_pos = 0;

	if (!mc || !line || n == 0)
		return NULL;

	/* check if this row has any cursors */
	for (i = 0; i < n; i++) {
		if (mc->cursors[i].row == row) {
			has_cursor = 1;
			break;
		}
	}

	if (!has_cursor)
		return NULL;

	marked = sbuf_make();

	/* insert markers at each cursor position */
	for (i = 0; i < n; i++) {
		if (mc->cursors[i].row == row) {
			int coff = mc->cursors[i].off;

			/* add text before cursor */
			if (coff > last_pos) {
				char *seg = uc_sub(line, last_pos, coff);
				sbuf_str(marked, seg);
				free(seg);
			}

			/* add cursor marker */
			sbuf_str(marked, "▌");
			last_pos = coff;
		}
	}

	/* add remaining text */
	char *seg = uc_sub(line, last_pos, -1);
	sbuf_str(marked, seg);
	free(seg);

	return sbuf_done(marked);
}

/* real-time character input for multi-cursor mode */
int mc_realtime_input(struct mc *mc, int cmd)
{
	struct sbuf *sb = sbuf_make();
	int c, i;
	int mc_n = mc->cursor_n;
	/* save original lines and positions for each cursor */
	char **orig_lines = malloc(mc_n * sizeof(char*));
	int *orig_offs = malloc(mc_n * sizeof(int));
	int *orig_rows = malloc(mc_n * sizeof(int));

	for (i = 0; i < mc_n; i++) {
		char *cln = lbuf_get(xb, mc->cursors[i].row);
		orig_lines[i] = cln ? uc_dup(cln) : uc_dup("\n");
		orig_rows[i] = mc->cursors[i].row;
		orig_offs[i] = mc->cursors[i].off;
	}

	/* read and apply characters in real-time */
	while (1) {
		c = vi_read();

		/* exit on Esc or Ctrl+C */
		if (c == TK_ESC || c == TK_CTL('c'))
			break;

		/* handle backspace */
		if (c == 127 || c == TK_CTL('h') || c == TK_CTL('?')) {
			if (sbuf_len(sb) > 0) {
				char *tmp = sbuf_buf(sb);
				int len = strlen(tmp);
				/* find start of last UTF-8 character */
				while (len > 0 && (tmp[len-1] & 0xC0) == 0x80)
					len--;
				if (len > 0)
					len--;
				sbuf_cut(sb, len);
			} else {
				continue;
			}
		} else {
			/* add character to buffer */
			sbuf_chr(sb, c);
		}

		/* apply current input to all cursors using ORIGINAL lines */
		/* process each unique row, merging cursors on same line */
		for (i = mc_n - 1; i >= 0; i--) {
			int crow = orig_rows[i];
			char *cln = orig_lines[i];
			struct sbuf *line_sb = sbuf_make();
			int last_pos = 0;
			int j;

			/* find all cursors on this row and merge their edits */
			for (j = 0; j < mc_n; j++) {
				if (orig_rows[j] == crow) {
					int coff_ins = orig_offs[j];

					/* handle special positioning */
					if (cmd == 'I')
						coff_ins = lbuf_indents(xb, crow);
					else if (cmd == 'A')
						coff_ins = lbuf_eol(xb, crow);
					else if (cmd == 'a')
						coff_ins = orig_offs[j] + 1;
					else if (cmd == 'i')
						coff_ins = orig_offs[j];

					if (cln && cln[0] == '\n')
						coff_ins = 0;

					/* add text from last_pos to coff_ins */
					if (coff_ins > last_pos) {
						char *segment = uc_sub(cln, last_pos, coff_ins);
						sbuf_str(line_sb, segment);
						free(segment);
					}

					/* insert the typed text */
					sbuf_str(line_sb, sbuf_buf(sb));
					last_pos = coff_ins;
				}
			}

			/* add remaining text after last cursor */
			if (cln) {
				char *segment = uc_sub(cln, last_pos, -1);
				sbuf_str(line_sb, segment);
				free(segment);
			}

			/* apply the merged edit */
			char *merged = sbuf_done(line_sb);
			lbuf_edit(xb, merged, crow, crow + 1);
			free(merged);

			/* skip other cursors on this row */
			while (i > 0 && orig_rows[i - 1] == crow)
				i--;
		}

		/* redraw screen */
		vi_drawagain(1, -1);
	}

	/* cleanup */
	for (i = 0; i < mc_n; i++)
		free(orig_lines[i]);
	free(orig_lines);
	free(orig_offs);
	free(orig_rows);
	sbuf_free(sb);

	return 0;
}
