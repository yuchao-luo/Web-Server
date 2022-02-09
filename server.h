#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "utils.h"
#include "http_request.h"

#ifndef PORT
#define PORT 9999
#endif

#ifndef BACKLOG
#define BACKLOG 2 /*maximum number of client connections*/
#endif

typedef struct server {
	// file descriptor of the socket in passive mode to wait for connections
	int listen_fd;
} server_t;

/**
 * Creates a socket for the server and makes it passive so
 * we can wait for connections on it later.
 */
int server_listen(server_t *server)
{
	int err = 0;
	struct sockaddr_in server_addr = { 0 };

	err = (server->listen_fd = socket(AF_INET, SOCK_STREAM, 0));

	if (err == -1) {
		printError("socket", "Failed to create socket endpoint");
		return err;
	}

	// setting SO_REUSEADDR enables us to reuse the address (bind to it) without
	// waiting for the socket to time out
	int enableReuse = 1;
	err = setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &enableReuse,
					 sizeof(int));

	if (err < 0) {
		printError("setsockopt", "setsockopt(SO_REUSEADDR) failed");
		return err;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	// bind socket to address
	err = bind(	server->listen_fd, (struct sockaddr*)&server_addr,
				sizeof(server_addr));

	if (err == -1) {
		printError("bind", "Failed to bind socket to address");
		return err;
	}

	// put socket in passive mode
	// BACKLOG: maximum number of concurrent connections
	//          default: 2
	err = listen(server->listen_fd, BACKLOG);

	if (err == -1) {
		printError("listen", "Failed to put socket in passive mode");
		return err;
	}

	printMessage("SERVER", "Server socket created and put in passive mode");

	return err;
}

int write_file(	const char *filename, char *buffer, const int append,
				const int binary, const int count)
{
	int err = 0;
	FILE *fp;

	if (append) {
		if (binary)
			fp = fopen(filename, "ab");
		else
			fp = fopen(filename, "a");
	}
	else {
		if (binary)
			fp = fopen(filename, "wb");
		else
			fp = fopen(filename, "w");
	}

	if (fp != NULL) {
		if (binary)
			err = fwrite(buffer, sizeof(char), count, fp);
		else
			err = fputs(buffer, fp);

		if (err == EOF) {
			printError("write file", "Failed to write to file");
			return err;
		}

		fclose(fp);

		return 0;
	}

	return -1;
}

/**
 * Reads file with given file name into the given buffer.
 */
int read_file(const char *filename, char **buffer, const int binary)
{
	int err = 0;
	long read = 0;
	FILE *fp;

	if (binary)
		fp = fopen(filename, "r");
	else
		fp = fopen(filename, "rb");

	if (fp != NULL) {
		// Go to the end of the file
		if (fseek(fp, 0L, SEEK_END) == 0) {
			// Get the size of the file
			long bufsize = ftell(fp);

			if ((err = bufsize) == -1) {
				printError("read file", "Failed to get the size of the file");
				return err;
			}

			// Allocate our buffer to that size
			if (binary) {
				*buffer = malloc(sizeof(char) * (bufsize + 1));
				memset(*buffer, 0, bufsize + 1);
			} else {
				*buffer = malloc(sizeof(char) * bufsize);
				memset(*buffer, 0, bufsize);
			}

			// Go back to the start of the file
			if ((err = fseek(fp, 0L, SEEK_SET)) != 0) {
				printError("read file", "Failed to rewind to beginning of file");
				return err;
			}

			// Reads file into buffer
			read = fread(*buffer, sizeof(char), bufsize, fp);

			if ((err = ferror(fp)) != 0) {
				printError("read file", "error reading file");
				return err;
			}
		}

		fclose(fp);

		return read;
	}

	return -1;
}

int server_parse_request(HTTP_Request *request, const char *readBuf)
{
	int err = 0;

	memset(request->path, 0, MAX_PATH_LENGTH * sizeof(char));

	if (readBuf[0] == 'G') {
		// GET
		request->method = GET;

		// 4 = length of "GET "
		size_t end_of_path = 4;

		// find where path ends
		while (readBuf[++end_of_path] != ' ');

		// set request path
		memcpy(request->path, readBuf + 4, end_of_path - 4);

		// GET requests don't have a body
		request->body = NULL;
	} else if (readBuf[0] == 'P') {
		// POST
		// TODO
		request->method = POST;
	} else {
		// Invalid request method
		err = -1;
	}

	return err;
}

/**
 * Creates and sends a response based on given request.
 */
int server_serve(int conn_fd)
{
	int err = 0;

	// default read buffer size: 64 KB
	const unsigned int readBufSize = 64 * 1024;
	char readBuf[readBufSize];
	HTTP_Request request;

	memset(readBuf, 0, readBufSize * sizeof(char));

	// read request into buffer
	read(conn_fd, readBuf, readBufSize);
	// try to parse the request
	err = server_parse_request(&request, readBuf);

	if (err == -1) {
		printError("parse request", "Failed to parse request");
		return err;
	}

	printf("Request Method: %s\n", request.method == GET ? "GET" : "POST");
	printf("Request Path: %s\n", request.path);

	err = write_file("log/server.log", readBuf, 1, 0, 0);

	if (err == -1) {
		printError("write log", "Failed to write received request to log");
		return err;
	}

	if (strcmp(request.path, "/") == 0) {
		char *writeBuf = NULL;
		char *response = NULL;
		char *header =  "HTTP/1.1 200 OK\r\n"
						"Content-Encoding: gzip\r\n"
						"Content-Type: text/html\r\n"
						"Content-Length: ";

		// read contents of HTML file to string
		long read = read_file("static/html/index.html.gz", &response, 1);

		if ((err = read) == -1) {
			// deallocate string buffer
			free(response);
			printError("serve", "failed to read HTML file");
			return err;
		}
		err = 0;

		// assemble response header and body
		char bodySize[16];
		sprintf(bodySize, "%ld", read);

		// buffer size is calculated as:
		// header size + content-length value size + 2 (CRLF) + response body size
		long bufSize = strlen(header) + read + strlen(bodySize) + 4;
		writeBuf = malloc(sizeof(char) * bufSize);
		snprintf(writeBuf, bufSize - read + 1, "%s%d\r\n\r\n", header, read);
		memcpy(writeBuf + bufSize - read, response, read);
		write_file("log/response.log", writeBuf, 0, 1, bufSize);

		// deallocate string buffer
		free(response);

		// send response
		write(conn_fd, writeBuf, bufSize);
		printMessage("SEND", "response sent");

		// clean up
		free(writeBuf);
	} else {
		printMessage("SERVER", "Request ignored");
	}

	return err;
}

/**
 * Accepts new connections and responds to requests.
 */
int server_accept(server_t *server)
{
	int err = 0;
	int conn_fd;
	socklen_t client_len;
	struct sockaddr_in client_addr;

	client_len = sizeof(client_addr);

	// accept connection
	err = (conn_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &client_len));

	if (err == -1) {
		printError("accept", "failed to accept connection");
		return err;
	}

	char msg[128];
	sprintf(msg, "New connection established @%ld", time(NULL));
	printMessage("SERVER", msg);

	err = server_serve(conn_fd);

	if (err == -1) {
		printError("serve", "error occurred while serving requested content");
		return err;
	}

	err = close(conn_fd);

	if (err == -1) {
		printError("close", "failed to close connection");
		return err;
	}

	return err;
}

#endif
