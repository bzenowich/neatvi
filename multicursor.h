/* multi-cursor support for neatvi */
#ifndef MULTICURSOR_H
#define MULTICURSOR_H

/* cursor position structure */
struct mc_cursor {
	int row;		/* cursor row */
	int off;		/* cursor offset within the row */
};

/* multi-cursor manager */
struct mc;

/* create a new multi-cursor manager */
struct mc *mc_make(void);

/* free multi-cursor manager */
void mc_free(struct mc *mc);

/* add a cursor at the specified position */
void mc_add(struct mc *mc, int row, int off);

/* remove all cursors */
void mc_clear(struct mc *mc);

/* get the number of active cursors */
int mc_count(struct mc *mc);

/* get cursor at index i (returns 0 on success, 1 on failure) */
int mc_get(struct mc *mc, int i, int *row, int *off);

/* set cursor at index i */
void mc_set(struct mc *mc, int i, int row, int off);

/* check if multi-cursor mode is active (more than 0 cursors) */
int mc_active(struct mc *mc);

/* remove cursor at index i */
void mc_remove(struct mc *mc, int i);

/* update all cursor positions after text insertion/deletion
 * pos: the line where modification occurred
 * n_del: number of lines deleted
 * n_ins: number of lines inserted
 * off: offset within the line (for single-line edits)
 * off_delta: change in offset (for single-line edits)
 */
void mc_adjust(struct mc *mc, int pos, int n_del, int n_ins, int off, int off_delta);

/* build a display line with visual cursor markers
 * returns a new string with markers inserted, or NULL if no cursors on this row
 * caller must free the returned string
 */
char *mc_build_display_line(struct mc *mc, char *line, int row);

/* real-time character input for multi-cursor mode
 * handles character-by-character input with live updates at all cursor positions
 * cmd: insert command ('i', 'a', 'I', 'A')
 * returns 0 on success, non-zero on error
 */
int mc_realtime_input(struct mc *mc, int cmd);

#endif
