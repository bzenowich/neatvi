/*
 * Server/Client implementation for neatvi
 * Provides vim-like --servername and --remote functionality
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "vi.h"

#define MAX_SERVERS 64
#define SOCKET_DIR "/tmp"
#define SOCKET_PREFIX "neatvi-"

static int server_fd = -1;		/* server socket fd */
static int client_fd = -1;		/* current client connection fd */
static char server_path[256];		/* path to server socket */
static char server_name[64];		/* server name */

/* get the socket path for a given server name */
static void server_sockpath(char *path, int len, char *name)
{
	snprintf(path, len, "%s/%s%s-%d", SOCKET_DIR, SOCKET_PREFIX,
		name, getuid());
}

/* initialize server mode: create and listen on UNIX socket */
int server_init(char *name)
{
	struct sockaddr_un addr;
	struct stat st;

	if (!name || !name[0])
		return -1;

	snprintf(server_name, sizeof(server_name), "%s", name);
	server_sockpath(server_path, sizeof(server_path), name);

	/* check if socket already exists */
	if (stat(server_path, &st) == 0) {
		/* try to connect to see if server is alive */
		if (server_check(name)) {
			fprintf(stderr, "neatvi: server '%s' already running\n", name);
			return -1;
		}
		/* stale socket, remove it */
		unlink(server_path);
	}

	server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("socket");
		return -1;
	}

	/* set non-blocking */
	fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", server_path);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(server_fd);
		server_fd = -1;
		return -1;
	}

	/* set socket permissions to user-only */
	chmod(server_path, 0600);

	if (listen(server_fd, 5) < 0) {
		perror("listen");
		close(server_fd);
		server_fd = -1;
		unlink(server_path);
		return -1;
	}

	return 0;
}

/* check if a server with given name is running */
int server_check(char *name)
{
	char path[256];
	int fd;
	struct sockaddr_un addr;

	server_sockpath(path, sizeof(path), name);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return 0;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return 0;
	}

	close(fd);
	return 1;
}

/* list all running neatvi servers */
void server_list(void)
{
	DIR *dir;
	struct dirent *ent;
	char suffix[32];
	int found = 0;

	snprintf(suffix, sizeof(suffix), "-%d", getuid());

	dir = opendir(SOCKET_DIR);
	if (!dir) {
		perror("opendir");
		return;
	}

	while ((ent = readdir(dir)) != NULL) {
		/* check if it starts with SOCKET_PREFIX and ends with our uid */
		if (strncmp(ent->d_name, SOCKET_PREFIX, strlen(SOCKET_PREFIX)) == 0) {
			char *suffix_pos = strstr(ent->d_name, suffix);
			if (suffix_pos && suffix_pos[strlen(suffix)] == '\0') {
				char *name_start = ent->d_name + strlen(SOCKET_PREFIX);
				char *name_end = suffix_pos;
				char name[64];
				int len = name_end - name_start;

				if (len > 0 && len < sizeof(name)) {
					memcpy(name, name_start, len);
					name[len] = '\0';

					/* verify server is still alive */
					if (server_check(name)) {
						printf("%s\n", name);
						found = 1;
					} else {
						/* clean up stale socket */
						char path[256];
						server_sockpath(path, sizeof(path), name);
						unlink(path);
					}
				}
			}
		}
	}

	closedir(dir);

	if (!found)
		printf("No servers running\n");
}

/* send a command to a server */
int server_send(char *name, char *cmd)
{
	char path[256];
	char buf[512];
	int fd;
	struct sockaddr_un addr;
	long nw = 0, nr;
	long len;

	if (!name || !name[0] || !cmd)
		return 1;

	server_sockpath(path, sizeof(path), name);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "neatvi: server '%s' not found\n", name);
		close(fd);
		return 1;
	}

	/* send command */
	len = strlen(cmd);
	while (nw < len) {
		nr = write(fd, cmd + nw, len - nw);
		if (nr <= 0) {
			perror("write");
			close(fd);
			return 1;
		}
		nw += nr;
	}

	/* send newline if not present */
	if (cmd[len - 1] != '\n')
		write(fd, "\n", 1);

	shutdown(fd, SHUT_WR);

	/* read response */
	nr = read(fd, buf, sizeof(buf) - 1);
	if (nr > 0) {
		buf[nr] = '\0';
		if (strncmp(buf, "OK", 2) != 0) {
			fprintf(stderr, "%s", buf);
			close(fd);
			return 1;
		}
	}

	close(fd);
	return 0;
}

/* accept incoming client connection (non-blocking) */
int server_accept(void)
{
	struct sockaddr_un addr;
	socklen_t len = sizeof(addr);

	if (server_fd < 0)
		return 0;

	/* close previous client if any */
	if (client_fd >= 0) {
		close(client_fd);
		client_fd = -1;
	}

	client_fd = accept(server_fd, (struct sockaddr *)&addr, &len);
	if (client_fd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		return 0;
	}

	/* set non-blocking */
	fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);

	return 1;
}

/* read command from client (blocking on current client fd) */
char *server_read(void)
{
	char buf[4096];
	int nr = 0;
	int total = 0;
	struct sbuf *sb;

	if (client_fd < 0)
		return NULL;

	/* set blocking for reading the command */
	fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) & ~O_NONBLOCK);

	sb = sbuf_make();

	while (total < sizeof(buf) - 1) {
		nr = read(client_fd, buf, 1);
		if (nr <= 0)
			break;
		total += nr;
		if (buf[0] == '\n')
			break;
		sbuf_chr(sb, buf[0]);
	}

	if (total == 0) {
		sbuf_free(sb);
		close(client_fd);
		client_fd = -1;
		return NULL;
	}

	return sbuf_done(sb);
}

/* send response to client */
void server_respond(char *msg)
{
	if (client_fd < 0)
		return;

	if (msg)
		write(client_fd, msg, strlen(msg));
	else
		write(client_fd, "OK\n", 3);

	close(client_fd);
	client_fd = -1;
}

/* cleanup server: close socket and remove file */
void server_cleanup(void)
{
	if (client_fd >= 0) {
		close(client_fd);
		client_fd = -1;
	}

	if (server_fd >= 0) {
		close(server_fd);
		server_fd = -1;
		unlink(server_path);
	}
}

/* check if running as server */
int server_isactive(void)
{
	return server_fd >= 0;
}

/* get server name */
char *server_getname(void)
{
	return server_name[0] ? server_name : NULL;
}
