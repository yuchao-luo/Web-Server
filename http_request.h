#ifndef HTTP_REQUEST
#define HTTP_REQUEST

#define MAX_PATH_LENGTH 512

enum HTTP_Method {
	GET,
	POST
};

typedef struct HTTP_Request {
	// HTTP Method: GET or POST
	int method;
	// Requested path: e.g. /index.html?query=example
	char path[MAX_PATH_LENGTH];
	// Request body: empty for GET requests
	char *body;
} HTTP_Request;

#endif
