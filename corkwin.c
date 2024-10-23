#include <winsock2.h>
#include <fcntl.h>
#include <stdio.h>
#include <conio.h>
#include <sys/stat.h>

char *base64_encode(char *in);
void usage(void);
int sock_connect(const char *hname, int port);
int main(int argc, char *argv[]);

#define BUFSIZE 4096

char linefeed[] = "\r\n\r\n";

/*
** base64.c
** Copyright (C) 2001 Tamas SZERB <toma@rulez.org>
*/

const static char base64[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* the output will be allocated automagically */
char*
base64_encode(char *in) {
	char *src, *end;
	char *buf, *ret;

	unsigned int tmp;

	int i,len;

	len = strlen(in);
	if (!in)
		return NULL;
	else
		len = strlen(in);

	end = in + len;

	buf = malloc(4 * ((len + 2) / 3) + 1);
	if (!buf)
		return NULL;
	ret = buf;


	for (src = in; src < end - 3;) {
		tmp = *src++ << 24;
		tmp |= *src++ << 16;
		tmp |= *src++ << 8;

		*buf++ = base64[tmp >> 26];
		tmp <<= 6;
		*buf++ = base64[tmp >> 26];
		tmp <<= 6;
		*buf++ = base64[tmp >> 26];
		tmp <<= 6;
		*buf++ = base64[tmp >> 26];
	}

	tmp = 0;
	for (i = 0; src < end; i++)
		tmp |= *src++ << (24 - 8 * i);

	switch (i) {
		case 3:
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
		break;
		case 2:
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
			*buf++ = '=';
		break;
		case 1:
			*buf++ = base64[tmp >> 26];
			tmp <<= 6;
			*buf++ = base64[tmp >> 26];
			*buf++ = '=';
			*buf++ = '=';
		break;
	}

	*buf = 0;
	return ret;
}

void
usage(void) {
	printf("Usage: corkwin <proxyhost> <proxyport> <desthost> <destport> [authfile]\n");
}

int
sock_connect (const char *hname, int port) {
	int fd;
	struct sockaddr_in addr;
	struct hostent *hent;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		return -1;

	hent = gethostbyname(hname);
	if (hent == NULL)
		addr.sin_addr.s_addr = inet_addr(hname);
	else
		memcpy(&addr.sin_addr, hent->h_addr, hent->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
		return -1;

	return fd;
}

/*
* connect.c -- Make socket connection using SOCKS4/5 and HTTP tunnel.
*
* Copyright (c) 2000-2006, 2012 Shun-ichi Goto
* Copyright (c) 2002, J. Grant (English Corrections)
*/

int
stdindatalen (void)
{
    DWORD len = 0;
    struct stat st;
    fstat( 0, &st );
    if ( st.st_mode & _S_IFIFO ) {
        /* in case of PIPE */
        if ( !PeekNamedPipe( GetStdHandle(STD_INPUT_HANDLE),
                             NULL, 0, NULL, &len, NULL) ) {
            if ( GetLastError() == ERROR_BROKEN_PIPE ) {
                /* PIPE source is closed */
                /* read() will detects EOF */
                len = 1;
            } else {
                fprintf(stderr, "PeekNamedPipe() failed, errno=%d\n",
                      GetLastError());
            }
        }
    } else if ( st.st_mode & _S_IFREG ) {
        /* in case of regular file (redirected) */
        len = 1;                        /* always data ready */
    } else if ( _kbhit() ) {
        /* in case of console */
        len = 1;
    }
    return len;
}

int
main (int argc, char *argv[]) {
	WSADATA wsadata;
    WSAStartup( 0x101, &wsadata);
	char uri[BUFSIZE] = "", buffer[BUFSIZE] = "", version[BUFSIZE] = "", descr[BUFSIZE] = "";
	char *host = NULL, *desthost = NULL, *destport = NULL;
	char *up = NULL, line[4096];
	int port, sent, setup, code, csock;
	fd_set rfd, sfd;
	struct timeval tv;
	ssize_t len;
	FILE *fp;

	port = 80;

	if (argc == 5 || argc == 6) {
		if (argc == 5) {
			host = argv[1];
			port = atoi(argv[2]);
			desthost = argv[3];
			destport = argv[4];
			up = getenv("CORKSCREW_AUTH");
		}
		if (argc == 6) {
			host = argv[1];
			port = atoi(argv[2]);
			desthost = argv[3];
			destport = argv[4];
			fp = fopen(argv[5], "r");
			if (fp == NULL) {
				fprintf(stderr, "Error opening %s: %s\n", argv[5], strerror(errno));
				exit(-1);
			} else {
				if (!fscanf(fp, "%4095s", line)) {
					fprintf(stderr, "Error reading auth file's content\n");
					exit(-1);
				}

				up = line;
				fclose(fp);
			}
		}
	} else {
		usage();
		exit(-1);
	}

	strncpy(uri, "CONNECT ", sizeof(uri));
	strncat(uri, desthost, sizeof(uri) - strlen(uri) - 1);
	strncat(uri, ":", sizeof(uri) - strlen(uri) - 1);
	strncat(uri, destport, sizeof(uri) - strlen(uri) - 1);
	strncat(uri, " HTTP/1.0", sizeof(uri) - strlen(uri) - 1);
	if (up != NULL) {
		strncat(uri, "\nProxy-Authorization: Basic ", sizeof(uri) - strlen(uri) - 1);
		strncat(uri, base64_encode(up), sizeof(uri) - strlen(uri) - 1);
	}
	strncat(uri, linefeed, sizeof(uri) - strlen(uri) - 1);

	csock = sock_connect(host, port);
	if(csock == -1) {
		fprintf(stderr, "Couldn't establish connection to proxy: %s\n", strerror(errno));
		exit(-1);
	}

	setmode(0, O_BINARY);
	setmode(1, O_BINARY);

	sent = 0;
	setup = 0;
	for(;;) {
		FD_ZERO(&sfd);
		FD_ZERO(&rfd);
		FD_SET(csock, &rfd);
		if ((setup == 0) && (sent == 0)) {
			FD_SET((SOCKET)csock, &sfd);
		}

		tv.tv_sec = 0;
		tv.tv_usec = 10*1000;;

		if(select(csock+1,&rfd,&sfd,NULL,&tv) == -1) {
			fprintf(stderr, "Test %d\n", WSAGetLastError());
			break;
		};

		if (0 < stdindatalen()) {
            FD_SET (0, &rfd);          /* fake */
        }

		/* there's probably a better way to do this */
		if (setup == 0) {
			if (FD_ISSET(csock, &rfd)) {
				len = recv(csock, buffer, (int) sizeof(buffer), 0);
				if (len<=0)
					break;
				else {
					memset(descr, 0, sizeof(descr));
					sscanf(buffer,"%s%d%[^\n]",version,&code,descr);
					if ((strncmp(version,"HTTP/",5) == 0) && (code >= 200) && (code < 300))
						setup = 1;
					else {
						if ((strncmp(version,"HTTP/",5) == 0) && (code >= 407)) {
						}
						fprintf(stderr, "Proxy could not open connection to %s: %s\n", desthost, descr);
						exit(-1);
					}
				}
			}

			if (FD_ISSET(csock, &sfd) && (sent == 0)) {
				len = send(csock, uri, (int) strlen(uri), 0);
				if (len<=0)
					break;
				else
					sent = 1;
			}
		} else {
			if (FD_ISSET(csock, &rfd)) {
				len = recv(csock, buffer, (int) sizeof(buffer), 0);
				if (len<=0) break;
				len = write(1, buffer, (int) len);
				if (len<=0) break;
			}

			if (FD_ISSET(0, &rfd)) {
				len = read(0, buffer, (int) sizeof(buffer));
				if (len<=0) break;
				len = send(csock, buffer, (int) len, 0);
				if (len<=0) break;
			}
		}
	}
	WSACleanup();
	exit(0);
}
