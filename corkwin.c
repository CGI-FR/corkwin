#include <conio.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

char *base64_encode(char *in);
void usage(void);
int __cdecl main(int argc, char **argv);

#define BUFSIZE 4096

char linefeed[] = "\r\n\r\n";

/*
** base64.c
** Copyright (C) 2001 Tamas SZERB <toma@rulez.org>
*/

const static char base64[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* the output will be allocated automagically */
char *base64_encode(char *in) {
  char *src, *end;
  char *buf, *ret;

  unsigned int tmp;

  int i, len;

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

void usage(void) {
  printf("Usage: corkwin <proxyhost> <proxyport> <desthost> <destport>\n");
}

/*
 * connect.c -- Make socket connection using SOCKS4/5 and HTTP tunnel.
 *
 * Copyright (c) 2000-2006, 2012 Shun-ichi Goto
 * Copyright (c) 2002, J. Grant (English Corrections)
 */

int stdindatalen(void) {
  DWORD len = 0;
  struct stat st;
  fstat(0, &st);
  if (st.st_mode & _S_IFIFO) {
    /* in case of PIPE */
    if (!PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), NULL, 0, NULL, &len,
                       NULL)) {
      if (GetLastError() == ERROR_BROKEN_PIPE) {
        /* PIPE source is closed */
        /* read() will detects EOF */
        len = 1;
      } else {
        fprintf(stderr, "PeekNamedPipe() failed, errno=%d\n", GetLastError());
      }
    }
  } else if (st.st_mode & _S_IFREG) {
    /* in case of regular file (redirected) */
    len = 1; /* always data ready */
  } else if (_kbhit()) {
    /* in case of console */
    len = 1;
  }
  return len;
}

int __cdecl main(int argc, char **argv) {
  WSADATA wsaData;
  int iResult;
  SOCKET ConnectSocket = INVALID_SOCKET;
  struct addrinfo *result = NULL, *ptr = NULL, hints;
  char uri[BUFSIZE] = "", buffer[BUFSIZE] = "", version[BUFSIZE] = "",
       descr[BUFSIZE] = "";
  char *host = NULL, *port = NULL, *desthost = NULL, *destport = NULL;
  char *up = NULL, line[4096];
  int sent, setup, code;
  fd_set rfd, sfd;
  struct timeval tv;
  ssize_t len;
  FILE *fp;

  port = "80";

  if (argc == 5) {
    host = argv[1];
    port = argv[2];
    desthost = argv[3];
    destport = argv[4];
  } else {
    usage();
    return -1;
  }

  // Initialize Winsock
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  // Resolve the server address and port
  iResult = getaddrinfo(host, port, &hints, &result);
  if (iResult != 0) {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    return 1;
  }

  // Attempt to connect to an address until one succeeds
  for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
      printf("socket failed with error: %ld\n", WSAGetLastError());
      WSACleanup();
      return 1;
    }

    // Connect to server.
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
      closesocket(ConnectSocket);
      ConnectSocket = INVALID_SOCKET;
      continue;
    }
    break;
  }

  freeaddrinfo(result);

  if (ConnectSocket == INVALID_SOCKET) {
    printf("Unable to connect to server!\n");
    WSACleanup();
    return 1;
  }

  strncpy(uri, "CONNECT ", sizeof(uri));
  strncat(uri, desthost, sizeof(uri) - strlen(uri) - 1);
  strncat(uri, ":", sizeof(uri) - strlen(uri) - 1);
  strncat(uri, destport, sizeof(uri) - strlen(uri) - 1);
  strncat(uri, " HTTP/1.0", sizeof(uri) - strlen(uri) - 1);
  strncat(uri, linefeed, sizeof(uri) - strlen(uri) - 1);

  iResult = send(ConnectSocket, uri, (int)strlen(uri), 0);
  if (iResult == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    closesocket(ConnectSocket);
    WSACleanup();
    return 1;
  }

  iResult = recv(ConnectSocket, buffer, sizeof(buffer), 0);
  if (iResult > 0) {
    memset(descr, 0, sizeof(descr));
    sscanf(buffer, "%s%d%[^\n]", version, &code, descr);
    if ((strncmp(version, "HTTP/", 5) == 0) && (code >= 200) && (code < 300))
      setup = 1;
    else {
      if ((strncmp(version, "HTTP/", 5) == 0) && (code >= 407)) {
      }
      fprintf(stderr, "Proxy could not open connection to %s: %s\n", desthost,
              descr);
      exit(-1);
    }
  } else if (iResult == 0)
    printf("Connection closed\n");
  else
    printf("recv failed with error: %d\n", WSAGetLastError());

  _setmode(_fileno(stdin), O_BINARY);
  _setmode(_fileno(stdout), O_BINARY);

  while (1) {
    FD_ZERO(&rfd);
    FD_SET(ConnectSocket, &rfd);

    tv.tv_sec = 0;
    tv.tv_usec = 10 * 1000;

    if (select(ConnectSocket + 1, &rfd, NULL, NULL, &tv) == -1) {
      fprintf(stderr, "select failed with error %d\n", WSAGetLastError());
      break;
    };

    if (0 < stdindatalen()) {
      FD_SET(_fileno(stdin), &rfd); /* fake */
    }

    if (FD_ISSET(ConnectSocket, &rfd)) {
      len = recv(ConnectSocket, buffer, (int)sizeof(buffer), 0);
      if (len <= 0)
        break;
      len = write(_fileno(stdout), buffer, (int)len);
      if (len <= 0)
        break;
    }

    if (FD_ISSET(_fileno(stdin), &rfd)) {
      len = read(_fileno(stdin), buffer, (int)sizeof(buffer));
      if (len <= 0)
        break;
      len = send(ConnectSocket, buffer, (int)len, 0);
      if (len <= 0)
        break;
    }
  }

  // cleanup
  closesocket(ConnectSocket);
  WSACleanup();

  return 0;
}
