#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "vi.h"

static volatile sig_atomic_t term_interrupted = 0;

static void term_sighandler(int sig)
{
	term_interrupted = 1;
}

static struct sbuf *term_sbuf;	/* output buffer if not NULL */
static int rows, cols;		/* number of terminal rows and columns */
static int win_beg, win_rows;	/* active window rows */
static struct termios termios;

void term_init(void)
{
	struct winsize win;
	struct termios newtermios;
	struct sigaction sa;

	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~(ICANON | ISIG);
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &newtermios);
	if (getenv("LINES"))
		rows = atoi(getenv("LINES"));
	if (getenv("COLUMNS"))
		cols = atoi(getenv("COLUMNS"));
	if (!ioctl(0, TIOCGWINSZ, &win)) {
		cols = win.ws_col;
		rows = win.ws_row;
	}
	cols = cols ? cols : 80;
	rows = rows ? rows : 25;
	term_str("\33[m");
	term_window(win_beg, win_rows > 0 ? win_rows : rows);

	/* Set up signal handler for server refresh interrupts */
	sa.sa_handler = term_sighandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;  /* No SA_RESTART - we want to interrupt poll() */
	sigaction(SIGUSR1, &sa, NULL);
}

void term_window(int row, int cnt)
{
	char cmd[64];
	win_beg = row;
	win_rows = cnt;
	if (row == 0 && win_rows == rows) {
		term_str("\33[r");
	} else {
		sprintf(cmd, "\33[%d;%dr", win_beg + 1, win_beg + win_rows);
		term_str(cmd);
	}
}

void term_done(void)
{
	term_str("\33[r");
	term_pos(rows - 1, 0);
	term_kill();
	term_commit();
	tcsetattr(0, 0, &termios);
}

void term_suspend(void)
{
	term_done();
	kill(0, SIGSTOP);
	term_init();
}

void term_record(void)
{
	if (!term_sbuf)
		term_sbuf = sbuf_make();
}

void term_commit(void)
{
	if (term_sbuf) {
		write(1, sbuf_buf(term_sbuf), sbuf_len(term_sbuf));
		sbuf_free(term_sbuf);
		term_sbuf = NULL;
	}
}

static void term_out(char *s)
{
	if (term_sbuf)
		sbuf_str(term_sbuf, s);
	else
		write(1, s, strlen(s));
}

void term_str(char *s)
{
	term_out(s);
}

void term_chr(int ch)
{
	char s[4] = {ch};
	term_out(s);
}

void term_kill(void)
{
	term_out("\33[K");
}

void term_room(int n)
{
	char cmd[16];
	if (n < 0)
		sprintf(cmd, "\33[%dM", -n);
	if (n > 0)
		sprintf(cmd, "\33[%dL", n);
	if (n)
		term_out(cmd);
}

void term_pos(int r, int c)
{
	char buf[32] = "\r";
	if (c < 0)
		c = 0;
	if (c >= term_cols())
		c = cols - 1;
	if (r < 0)
		sprintf(buf, "\r\33[%d%c", abs(c), c > 0 ? 'C' : 'D');
	else
		sprintf(buf, "\33[%d;%dH", win_beg + r + 1, c + 1);
	term_out(buf);
}

int term_rowx(void)
{
	return rows;
}

int term_rows(void)
{
	return win_rows;
}

int term_cols(void)
{
	return cols;
}

static char ibuf[4096];		/* input character buffer */
static char icmd[4096];		/* read after the last term_cmd() */
static int ibuf_pos, ibuf_cnt;	/* ibuf[] position and length */
static int icmd_pos;		/* icmd[] position */

/* read s before reading from the terminal */
void term_push(char *s, int n)
{
	n = MIN(n, sizeof(ibuf) - ibuf_cnt);
	memcpy(ibuf + ibuf_cnt, s, n);
	ibuf_cnt += n;
}

/* return a static buffer containing inputs read since the last term_cmd() */
char *term_cmd(int *n)
{
	*n = icmd_pos;
	icmd_pos = 0;
	return icmd;
}

int term_read(void)
{
	struct pollfd ufds[2];
	int nfds, n, c;
	int server_fd;
	static int stdin_eof = 0;  /* track if stdin hit EOF */

	if (ibuf_pos >= ibuf_cnt) {
		/* check if we were interrupted by signal before blocking */
		if (term_interrupted) {
			term_interrupted = 0;
			return 0;
		}

		nfds = 0;

		/* poll stdin only if it hasn't hit EOF */
		if (!stdin_eof) {
			ufds[nfds].fd = 0;
			ufds[nfds].events = POLLIN;
			nfds++;
		}

		/* also poll server socket if active */
		server_fd = server_getfd();
		if (server_fd >= 0) {
			ufds[nfds].fd = server_fd;
			ufds[nfds].events = POLLIN;
			nfds++;
		}

		/* if no fds to poll, stdin is at EOF and no server active */
		if (nfds == 0)
			return -1;

		/* wait for either stdin or server socket */
		if (poll(ufds, nfds, -1) <= 0) {
			/* if interrupted by signal (server refresh), return to vi loop */
			if (term_interrupted) {
				term_interrupted = 0;
				return 0;
			}
			return -1;
		}

		/* also check after poll returns */
		if (term_interrupted) {
			term_interrupted = 0;
			return 0;
		}

		/* handle server requests if server socket is ready */
		if (server_fd >= 0) {
			int server_idx = stdin_eof ? 0 : 1;
			if (ufds[server_idx].revents & POLLIN) {
				server_handle();
				/* return immediately to allow vi loop to refresh screen */
				/* returning 0 will cause vi loop to continue and check xserver_refresh */
				if (stdin_eof || !(ufds[0].revents & POLLIN))
					return 0;
			}
		}

		/* read a single input character from stdin */
		if (!stdin_eof) {
			n = read(0, ibuf, 1);
			if (n <= 0) {
				stdin_eof = 1;  /* stdin closed/EOF */
				/* if server active, keep running; otherwise exit */
				if (server_fd >= 0)
					return 0;  /* return to vi loop, will poll again */
				return -1;
			}
			ibuf_cnt = n;
			ibuf_pos = 0;
		}
	}
	c = ibuf_pos < ibuf_cnt ? (unsigned char) ibuf[ibuf_pos++] : -1;
	if (icmd_pos < sizeof(icmd))
		icmd[icmd_pos++] = c;
	return c;
}

/* return a static string that changes text attributes from old to att */
char *term_seqattr(int att, int old)
{
	static char buf[128];
	char *s = buf;
	int fg = SYN_FG(att);
	int bg = SYN_BG(att);
	if (att == old)
		return "";
	s += sprintf(s, "\33[");
	if (att & SYN_BD)
		s += sprintf(s, ";1");
	if (att & SYN_IT)
		s += sprintf(s, ";3");
	else if (att & SYN_RV)
		s += sprintf(s, ";7");
	if (SYN_FGSET(att)) {
		if ((fg & 0xff) < 8)
			s += sprintf(s, ";%d", 30 + (fg & 0xff));
		else
			s += sprintf(s, ";38;5;%d", (fg & 0xff));
	}
	if (SYN_BGSET(att)) {
		if ((bg & 0xff) < 8)
			s += sprintf(s, ";%d", 40 + (bg & 0xff));
		else
			s += sprintf(s, ";48;5;%d", (bg & 0xff));
	}
	s += sprintf(s, "m");
	return buf;
}

char *term_seqkill(void)
{
	return "\33[K";
}
