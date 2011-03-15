#include <stdlib.h>
#include <stdio.h>
#include "core.h"	/* for server_init() */
#include "myev.h"	/* for myev_run() */

int
main ()
{
	struct irc_server *server_state;
	fprintf(stderr, "ircdaemon: first testrun...\n");
	server_state = server_init();
	if (!server_state) return EXIT_FAILURE;
	myev_run();
	server_shutdown(server_state);
	return EXIT_SUCCESS;
}
