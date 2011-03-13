#include <stdlib.h>
#include <stdio.h>
#include "irc.h"	/* for irc_server_init() */
#include "myev.h"	/* for myev_run() */

int
main ()
{
	struct irc_server *server_state;
	fprintf(stderr, "ircdaemon: first testrun...\n");
	server_state = irc_server_init();
	if (!server_state) return EXIT_FAILURE;
	myev_run();
	irc_server_cleanup(server_state);
	return EXIT_SUCCESS;
}
