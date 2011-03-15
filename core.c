/* core.c - irc server core functionality */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "irc.h"
#include "core.h"
#include "hash.h"
#include "net.h"
#include "myev.h"

/* core callbacks initialiser */
static int
_callback_init (struct irc_server *server)
{
	hash_init(&server->commands);
	if (hash_insert(&server->commands, "PING", irc_cmd_ping)) return -1;
	if (hash_insert(&server->commands, "PONG", irc_cmd_pong)) return -1;
	if (hash_insert(&server->commands, "NICK", irc_cmd_nick)) return -1;
	if (hash_insert(&server->commands, "MOTD", irc_cmd_motd)) return -1;
	if (hash_insert(&server->commands, "USER", irc_cmd_user)) return -1;
	return 0;
}

/* irc_server initialiser */
struct irc_server *
server_init ()
{
	struct irc_server *server;

	/* initialise state structure */
	server = malloc(sizeof(*server));
	if (!server) return NULL;
	memset(server, 0, sizeof(*server));

	/* initialise hash tables */
	hash_init(&server->users);
	hash_init(&server->chans);

	/* bind listener socket */
	server->sockfd = net_bind(IRC_HOST, IRC_DEFAULT_PORT);
	if (server->sockfd == -1)
	{
		free(server);
		return NULL;
	}
	
	/* here we need to create ev watchers, one to watch for
	 * connection requests and a repeating timer (for idle timers) */
	myev_init();
	myev_server_init(server);

	/* load command specific callbacks */
	if (_callback_init(server))
	{
		free(server);
		return NULL;
	}

	/* done! */
	return server;
}

/* irc server shutdown/cleanup */
void
server_shutdown (struct irc_server *server)
{
	struct irc_user **users;
	struct irc_channel **chans;
	int i;

	/* first, boot all the users
	 * channels depend on users so they should be cleared
	 * via kicking all users */
	users = (struct irc_user **)hash_values(&server->users);
	for (i = 0; users && users[i]; ++i)
		user_quit(users[i], "Server going down!");
	free(users);
	hash_assert(&server->users);
	hash_assert(&server->chans);

	/* clean command callback hash table */
	hash_free(&server->commands);

	/* NB: we are called only from main() and
	 * as a result of myev_server_fini() breaking
	 * the event loop */
	/***myev_server_fini(server);***/

	/* close socket */
	close(server->sockfd);

	/* free memory */
	free(server);
}

/* link two users, helps forwarding "QUIT" and "NICK" messages to
 * users who can 'SEE' the user and ought to be informed
 * would be useful for KILL, AWAY, etc. too */
struct user_user *
uu_link (struct irc_user *a, struct irc_user *b)
{
	struct user_user *link;

	link = hash_lookup(&a->assoc_users, b->nick);
	if (link)
	{
		link->refcount++;
	}
	else
	{
		/* new link */
		link = malloc(sizeof(*link));
		if (!link) return NULL;
		/* we dont want any one way linkage
		 * ... so we have to be careful here */
		if (hash_insert(&a->assoc_users, b->nick, link))
		{
			free(link);
			return NULL;
		}
		if (hash_insert(&b->assoc_users, a->nick, link))
		{
			assert(hash_remove(&a->assoc_users, b->nick) == link);
			free(link);
			return NULL;
		}
		link->user[0] = a;
		link->user[1] = b;
		link->refcount = 1;
	}
	return link;
}

/* unlink two users */
void
uu_unlink (struct irc_user *a, struct irc_user *b)
{
	struct user_user *link;

	/* note the potential SEGFAULT here?
	 * that's how i roll. uu_unlink() should NEVER be called
	 * with 2 users that are not linked anyway. If it *is* called
	 * in such a case, SEGFAULT reveals the problem!
	 * think of it as an implied assert() :-) */
	link = hash_lookup(&a->assoc_users, b->nick);
	link->refcount--;
	if (!link->refcount)
	{
		/* refcount zero, unlink proper!
		 * also, these assert()s that im throwing everywhere should
		 * help me spot bugs with the admittedly clumsy handling of
		 * user<->user linkage and channel<->user linkage */
		assert(hash_remove(&a->assoc_users, b->nick) == link);
		assert(hash_remove(&b->assoc_users, a->nick) == link);
		free(link);
	}
}

/* uc_link() is VITAL - here we not only link user and channel
 * but make calls to uu_link() to link the new user with existing
 * users in the channel */
struct chan_user *
uc_link (struct irc_user *user, struct irc_channel *chan)
{
	struct chan_user *chan_uref, **users;
	int i, j, fail;

	/* in order to protect the integrity of our complex
	 * data system, a failed uu_link() means we must
	 * undo any uu_link()s that did succeed...
	 * this sort of stuff is annoying because 99.99% of the
	 * time these functions will not fail, its only possible
	 * if malloc() returns ENOMEM but we ought to write code
	 * that remains stable even in weird corner cases */
	users = (struct chan_user **)hash_values(&chan->users);
	if (!users) return NULL;
	for (i = 0; users[i]; ++i)
	{
		if (!uu_link(user, users[i]->user))
		{
			for (j = 0; j < i; ++j)
			{
				uu_unlink(user, users[j]->user);
			}
			free(users);
			return NULL;
		}
	}

	/* whew! user<->user links updated, now link user<->chan */
	chan_uref = malloc(sizeof(*chan_uref));
	if (!chan_uref)
	{
		/* perhaps we should macro this?? nah.. */
		for (j = 0; j < i; ++j)
		{
			uu_unlink(user, users[j]->user);
		}
		free(users);
		return NULL;
	}

	chan_uref->has_chanop = 0;
	chan_uref->user = user;
	chan_uref->chan = chan;
	
	/* memory management in C is painful */
	if (hash_insert(&chan->users, user->nick, chan_uref))
	{
		for (j = 0; j < i; ++j)
		{
			uu_unlink(user, users[j]->user);
		}
		free(users);
		free(chan_uref);
		return NULL;
	}
	if (hash_insert(&user->chans, chan->name, chan_uref))
	{
		for (j = 0; j < i; ++j)
		{
			uu_unlink(user, users[j]->user);
		}
		free(users);
		free(chan_uref);
		hash_remove(&chan->users, user->nick);
		return NULL;
	}

	/* i *think* we've succesfully linked user and channel */
	chan->refcount++;
	return chan_uref;
}

/* unlink user and channel and unlink user from users in
 * the channel */
void
uc_unlink (struct irc_user *user, struct irc_channel *chan)
{
	struct chan_user *link, **users;
	int i;

	chan->refcount--;
	/* unlink user<->chan first */
	link = hash_remove(&user->chans, chan->name);
	assert(link == hash_remove(&chan->users, user->nick));
	free(link);
	/* now update user<->user linkage */
	users = (struct chan_user **)hash_values(&chan->users);
	for (i = 0; users && users[i]; ++i)
		uu_unlink(user, users[i]->user);
	free(users);
	/* remove channel if orphaned */
	if (!chan->refcount)
		chan_free(chan);
}

/* initialise a new channel - this will be called when
 * a user JOINs a nonexistant channel
 * int return value for failure/success */
int
chan_init (const char *name, struct irc_user *user)
{
	struct chan_user *chan_uref;
	struct irc_channel *new_chan;

	new_chan = malloc(sizeof(*new_chan));
	if (!new_chan) return -1;
	memset(new_chan, 0, sizeof(*new_chan));
	new_chan->server = user->server;
	hash_init(&new_chan->users);

	/* copy channel name to channel structure */
	strncpy(new_chan->name, name, CHAN_MAX_NAME);
	new_chan->name[CHAN_MAX_NAME - 1] = 0;
	
	/* link user and channel */
	if (!(chan_uref = uc_link(user, new_chan)))
	{
		free(new_chan);
		return -1;
	}
	/* channel creator always starts with chanop */
	chan_uref->has_chanop = 1;

	/* add channel to server's chan table */
	if (hash_insert(&user->server->chans, name, new_chan))
	{
		hash_remove(&user->chans, name);
		hash_remove(&new_chan->users, user->nick);
		free(chan_uref);
		free(new_chan);
		return -1;
	}
	
	/* channel created succesfully! */
	return 0;
}

/* destroy a channel */
void
chan_free (struct irc_channel *chan)
{
	struct irc_server *server;
	int i;

	/* remove chan from server chan table */
	assert((server = chan->server) != NULL);
	assert(hash_remove(&server->chans, chan->name) == chan);

	/* we are called from uc_unlink() in the zero refcount case
	 * therefore we neednt unlink users from the hash table! */
	hash_assert(&chan->users);

	/* free banmask strings */
	for (i = 0; i < CHAN_MAX_BANS; ++i)
		free(chan->bans[i]);
	
	/* free channel structure */
	free(chan);
}

/* initialise user data structure and link with server
 * this is called only when a new user connects */
void
user_init (int sfd, struct sockaddr_in *address,
			struct irc_server *server)
{
	struct irc_user *user;
	char *hostname;

	/* initialise user structure */
	user = malloc(sizeof(*user));
	if (!user) return;
	memset(user, 0, sizeof(*user));
	user->sockfd = sfd;
	user->server = server;
	hash_init(&user->chans);
	hash_init(&user->assoc_users);

	hostname = net_name_from_address(address);
	if (hostname)
	{
		strncpy(user->host, hostname, USER_MAX_HOST);
		user->host[USER_MAX_HOST - 1] = 0;
		fprintf(stderr, "client connected: %s\n", hostname);
		free(hostname);
	}
	else
	{
		strcpy(user->host, "unknown");
	}
	
	/* setup watcher -- trash the new user if this fails */
	if (myev_user_init(user))
	{
		free(user);
		close(sfd);
	}

	/* initialise user last active timestamp */
	user->time = time(NULL);
}

/* unlink hash table references, flag for user_free() */
void
user_quit (struct irc_user *user, const char *reason)
{
	struct irc_server *server;
	struct irc_channel **chans;
	struct irc_user **users;
	int i;

	/* some of these cleanup blocks require a user with a nickname */
	if (user->nick[0])
	{
		/* go through associated users and report the QUIT */
		users = (struct irc_user **)hash_values(&user->assoc_users);
		for (i = 0; users && users[i]; ++i)
			irc_rquit(user, users[i], reason);
		free(users);

		/* look thru the channels table and unlink
		 * cu_unlink will also make calls to uu_unlink so we
		 * don't need to do that ourselves */
		chans = (struct irc_channel **)hash_values(&user->chans);
		for (i = 0; chans && chans[i]; ++i)
			uc_unlink(user, chans[i]);
		free(chans);
		hash_assert(&user->chans);
		hash_assert(&user->assoc_users);
	
		/* remove nick from server hash table */
		assert((server = user->server) != NULL);
		assert(hash_remove(&server->users, user->nick) == user);
	}

	/* NB: we dont free anything here, instead we set user->quit to nonzero
	 * and then myev.c:_user_cb() does if(user->quit) user_free(user); */
	user->quit = 1;
}

/* free user data structure */
void
user_free (struct irc_user *user)
{	
	/* close user's network socket */
	close(user->sockfd);
	
	/* return sendq to pool if present */
	if (user->sendbuf)
		net_sendq_free(user->sendbuf);
	
	/* unset/free I/O event watcher */
	myev_user_fini(user);

	/* free user structure */
	free(user);
}

/* print format string to user buffer, attempt to flush */
ssize_t
user_sendf (struct irc_user *user, const char *fmt, ...)
{
	va_list ap;
	struct sendq *mybuf;
	char *p;
	ssize_t n, max_size;

	/* check for sendQ already present, if not, allocate one
	 * and set the write bit on the event handler */
	if (!user->sendbuf)
	{
		user->sendbuf = net_sendq_alloc();
		if (!user->sendbuf) return -1;	/* bail! */
		myev_set_rw(user);
	}
	mybuf = user->sendbuf;
	
	/* vsnprintf magic! */
	p = mybuf->buffer + mybuf->index;
	max_size = SENDQ_BUF_SZ - mybuf->index - 1;
	va_start(ap, fmt);
	n = vsnprintf(p, max_size, fmt, ap);
	va_end(ap);
	if (n < 0) return -1;
	/* data may have been truncated */
	if (n > max_size)
		mybuf->index += max_size;
	else
		mybuf->index += n;
}

/* user_sendl, without the newline (doesnt flush either) */
ssize_t
user_send (struct irc_user *user, const char *message)
{
	return user_sendf(user, "%s", message);
}

/* user_sendl, queue a message + newline char to be sent to user */
ssize_t
user_sendl (struct irc_user *user, const char *message)
{
	return user_sendf(user, "%s\n", message);
}

/* flush user's sendQ buffer */
ssize_t
user_flush (struct irc_user *user)
{
	int sock;
	struct sendq *mybuf;
	ssize_t offset;

	sock = user->sockfd;
	mybuf = user->sendbuf;
	offset = net_send(sock, mybuf);
	if (!mybuf->index)
	{
		/* entire buffer flushed, return sendQ to pool */
		user->sendbuf = NULL;
		net_sendq_free(mybuf);

		/* block EV_WRITE events */
		myev_set_ro(user);
	}
	return offset;
}

/* CORE FUNCTION */
void
user_read (struct irc_user *user)
{
	char *command, **args, *p, *q, *next;
	int nr_args, i;
	void (*handler)(struct irc_user *, int, char **);
	struct irc_server *server;
	
	/* set time stamp */
	user->time = time(NULL);
	p = user->recvbuf.buffer;
	/* strip newline */
	next = strchrnul(p, '\n');
	if (*next == '\n')
	{
		/* blasted \r characters... */
		if (*(next - 1) == '\r') *(next - 1) = 0;
		*(next++) = 0;
	}
	command = p;
	p = strchrnul(p, ' ');
	nr_args = 0;
	args = NULL;
	if (*p)
	{
		/* tokenise arguments */
		*(p++) = 0;
		nr_args = 1;
		q = p;
		while (*q != ':' && *q)
		{
			if (*(q++) == ' ') nr_args++;
		}
		args = malloc(nr_args * sizeof(*args));
		if (!args)
		{	/* bail!! */
			user->recvbuf.index = 0;
			return;
		}
		for (i = 0; i < nr_args - 1; ++i)
		{
			args[i] = p;
			p = strchrnul(p, ' ');
			*(p++) = 0;
		}
		args[i] = p;
	}

	/* verbose logging */
	fprintf(stderr, "user requested %s with %d args: ", command, nr_args);
	for (i = 0; i < nr_args; ++i)
		fprintf(stderr, "%s ", args[i]);
	fprintf(stderr, "\n");
	
	/* delegate to command specific handler */
	server = user->server;
	handler = hash_lookup(&server->commands, command);
	if (handler)
	{
		/* this is sum srs magic rite harr */
		(*handler)(user, nr_args, args);
	}
	else
	{	/* bail */
		user_sendf(user, ":%s %d %s %s :Unknown command\n",
					IRC_SERVER_NAME, IRC_ERROR_404, user->nick, command);
		user_flush(user);
	}
	free(args);

	/* BUG: we were throwing away data if a client sent more than one line
	 * at once, FIX: recursion */
	p = user->recvbuf.buffer;
	q = user->recvbuf.buffer + user->recvbuf.index;
	i = q - next;
	user->recvbuf.index = i;
	if (i)
	{
		memmove(p, next, i);
		user_read(user);
	}
}
