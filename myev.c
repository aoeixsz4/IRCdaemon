#include <stdlib.h>
#include <stdio.h>
#include <ev.h>
#include <netinet/in.h>
#include "myev.h"
#include "net.h"	/* for net_accept() etc */

static struct ev_loop *_loop;

static void
_server_cb (struct ev_loop *loop, struct ev_io *w, int revents)
{
	int sfd;
	struct irc_server *server;
	struct sockaddr_in address;
	server = w->data;
	
	/* accept new connection */
	sfd = net_accept(server->sockfd, &address);
	if (sfd == -1) return;	/* huuuuh?? */
	
	/* initialise user */
	user_init(sfd, &address, server);

	fprintf(stderr, "_server_cb(): accepted new connection: %d\n", sfd);
}

static void
_user_cb (struct ev_loop *loop, struct ev_io *w, int revents)
{
	struct irc_server *server;
	struct irc_user *user;
	ssize_t ret;
	user = w->data;
	server = user->server;

	ret = 0;
	/* is there data waiting to be read? */
	if (revents & EV_READ)
	{
		ret = net_recv(user->sockfd, &user->recvbuf);
		/* EOF */
		if (ret == 0)
		{
			user_quit(user);
			fprintf(stderr, "_user_cb(): disconnected socket %d\n",
					user->sockfd);
		}
		if (ret > 0)
		{
			fprintf(stderr, "_user_cb(): read %d bytes from socket %d\n",
						ret, user->sockfd);
			user_read(user);
		}
	}
	
	/* is the socket writable and is there data we want to send? */
	else if (revents & EV_WRITE && user->sendbuf)
	{
		ret = user_flush(user);
	}

	/* unexpected return from net_send/recv or user has quit */
	if (ret == -1 || user->quit)
	{
		fprintf(stderr, "_user_cb(): disconnected socket %d\n",
					user->sockfd);

		if (!user->quit) user_quit(user);
		user_free(user);
	}
}


void
myev_init ()
{
	_loop = ev_default_loop(0);
}

void
myev_run ()
{
	ev_run(_loop, 0);
}

int
myev_server_init (struct irc_server *server)
{
	struct ev_io *serv_watcher;
	int sfd;

	sfd = server->sockfd;
	/* allocate watcher struct */
	serv_watcher = malloc(sizeof(*serv_watcher));
	if (!serv_watcher) return -1;
	server->ev_w = serv_watcher;
	
	/* init watcher */
	serv_watcher->data = server;
	server->ev_w = serv_watcher;
	ev_io_init(serv_watcher, _server_cb, sfd, EV_READ);
	ev_io_start(_loop, serv_watcher);
	return 0;
}

void
myev_server_fini (struct irc_server *server)
{
	ev_io_stop(_loop, server->ev_w);
	free(server->ev_w);
	ev_break(_loop, EVBREAK_ALL);
}

int
myev_user_init (struct irc_user *user)
{
	struct ev_io *user_watcher;
	int sfd;

	sfd = user->sockfd;
	/* allocate watcher struct */
	user_watcher = malloc(sizeof(*user_watcher));
	if (!user_watcher) return -1;

	/* init watcher */
	user_watcher->data = user;
	user->ev_w = user_watcher;

	/* we should set EV_READ initially, then set EV_WRITE whenever
	 * we actually have data waiting to send to the client */
	ev_io_init(user_watcher, _user_cb, sfd, EV_READ);
	ev_io_start(_loop, user_watcher);
	return 0;
}

void
myev_user_fini (struct irc_user *user)
{
	ev_io_stop(_loop, user->ev_w);
	free(user->ev_w);
}

void
myev_set_rw (struct irc_user *user)
{
	int fd;
	struct ev_io *w;
	fd = user->sockfd;
	w = user->ev_w;
	ev_io_stop(_loop, w);
	ev_io_set(w, fd, EV_READ | EV_WRITE);
	ev_io_start(_loop, w);
}

void
myev_set_ro (struct irc_user *user)
{
	int fd;
	struct ev_io *w;
	fd = user->sockfd;
	w = user->ev_w;
	ev_io_stop(_loop, w);
	ev_io_set(w, fd, EV_READ);
	ev_io_start(_loop, w);
}
