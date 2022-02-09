#include "server.h"

int main(int argc, char const *argv[])
{
	int err = 0;
	server_t server = { 0 };

	err = server_listen(&server);
	if(err) {
		printf("Failed to listen on address 0.0.0.0:%d\n", PORT);
		return err;
	}

	char msg[128];
	sprintf(msg, "Listening for connections at 0.0.0.0:%d", PORT);
	printMessage("SERVER", msg);

	for(;;) {
		err = server_accept(&server);
		if(err)
		{
			printf("Failed accepting connection\n");
			return err;
		}
	}

	err = close(server.listen_fd);
	if(err) {
		printError("close", "Failed to close socket");
		return err;
	}

	return 0;
}
