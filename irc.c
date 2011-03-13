/* irc.c - irc server core functionality */
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
#include "hash.h"
#include "net.h"
#include "myev.h"

/* PING */
static void
_ping (struct irc_user *user, int nr_args, char **args)
{
	char *origin;
	if (!nr_args)
	{
		user_sendf(user, ":%s %3d %s :No origin specified\n",
					IRC_SERVER_NAME, IRC_ERROR_NOORIG, user->nick);
	}
	else
	{
		origin = args[0];
		if (origin[0] == ':') origin++;
		user_sendf(user, ":%s PONG %s :%s\n",
					IRC_SERVER_NAME, IRC_SERVER_NAME, origin);
	}
}

/* PONG */
static void
_pong (struct irc_user *user, int nr_args, char **args)
{
	if (!nr_args)
	{
		user_sendf(user, ":%s %3d %s :No origin specified\n",
					IRC_SERVER_NAME, IRC_ERROR_NOORIG, user->nick);
	}
	/* else NOOP */
}

/* MOTD */
static void
_motd (struct irc_user *user, int nr_args, char **args)
{
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_BEGIN,
				user->nick, "- ircdaemon message of the day -");
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_PIECE,
				user->nick, "- bla bla bla -");
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_END,
				user->nick, "- enjoy your stay! -");
}

/* welcome message */
static void
_welcome (struct irc_user *user)
{
	user_sendf(user, ":%s %3d %s :%s\n",
				IRC_SERVER_NAME, IRC_WELCOME, user->nick,
				WELCOME_MESSAGE);
	_motd (user, 0, NULL);
}

/* USER */
static void
_user (struct irc_user *user, int nr_args, char **args)
{
	char *ident;
	int i, reg;
	if (nr_args < 4)
	{
		user_sendf(user, ":%s %3d %s :Not enough parameters\n",
					IRC_SERVER_NAME, IRC_ERROR_FEWPARAM, user->nick);
		return;
	}
	ident = args[0];
	for (i = 0; ident[i]; ++i)
	{
		if (!strchr(ALPHANUMPLUS, ident[i]))
		{
			user_sendf(user, "ERROR :Closing Link: %s[%s] (NAUGHTY!)\n",
						user->nick, user->host);
			user_quit(user);
		}
	}
	/* check registration status */
	if (user->nick[0] && !user->ident[0])
	{
		reg = 1;
	}
	else
	{
		reg = 0;
	}
	strncpy(user->ident, ident, USER_MAX_IDENT);
	user->ident[USER_MAX_IDENT - 1] = 0;
	if (reg)
	{
		_welcome(user);
	}
}

/* NICK */
static void
_nick (struct irc_user *user, int nr_args, char **args)
{
	char *nick, *oldnick;
	int i, reg;
	struct irc_server *server;
	if (!nr_args)
	{
		user_sendf(user, ":%s %3d %s :Not enough parameters\n",
					IRC_SERVER_NAME, IRC_ERROR_FEWPARAM, user->nick);
		return;
	}
	nick = args[0];
	if (!strchr(ALPHAPLUS, nick[0]))
	{
		user_sendf(user, ":%s %3d %s :Erroneous nickname\n",
					IRC_SERVER_NAME, IRC_ERROR_BADNICK, nick);
		return;
	}
	for (i = 1; nick[i]; ++i)
	{
		if (!strchr(ALPHANUMPLUS, nick[i]))
		{
			user_sendf(user, ":%s %3d %s :Erroneous nickaname\n",
						IRC_SERVER_NAME, IRC_ERROR_BADNICK, nick);
		}
		return;
	}
	/* check if the user is registered and if not, if this NICK command
	 * will complete its registration */
	if (user->ident[0] && !user->nick[0])
	{
		reg = 1;
	}
	else
	{
		reg = 0;
	}
	/* remove old nick from hash table */
	server = user->server;
	hash_remove(&server->users, user->nick);
	oldnick = strdup(user->nick);
	if (!oldnick)
	{
		user_quit(user);
		return;
	}
	strncpy(user->nick, nick, USER_MAX_NICK);
	user->nick[USER_MAX_NICK - 1] = 0;
	if (hash_insert(&server->users, user->nick, user))
	{
		user_quit(user);
		free(oldnick);
		return;
	}
	
	if (reg)
	{
		/* send welcome message or something */
		_welcome(user);
	}
	else if (!user->ident[0])
	{
		/* reply for unregistered user */
		user_sendf(user, "PING :%8x\n", user);
	}
	else
	{
		/* confirm nick change */
		user_sendf(user, ":%s!%s@%s NICK :%s\n", oldnick, user->ident,
					user->host, user->nick);
	}
	free(oldnick);
}

/* core callbacks initialiser */
static int
_callback_init (struct irc_server *server)
{
	hash_init(&server->commands);
	if (hash_insert(&server->commands, "PING", _ping)) return -1;
	if (hash_insert(&server->commands, "PONG", _pong)) return -1;
	if (hash_insert(&server->commands, "NICK", _nick)) return -1;
	if (hash_insert(&server->commands, "MOTD", _motd)) return -1;
	if (hash_insert(&server->commands, "USER", _user)) return -1;
	return 0;
}

/* irc_server initialiser */
struct irc_server *
irc_server_init ()
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
	if (_callback_init(server)) return NULL;

	/* done! */
	return server;
}

/* irc server shutdown/cleanup */
void
irc_server_cleanup (struct irc_server *server)
{
	struct irc_user **users;
	struct irc_channel **chans;
	int i;

	/* first, boot all the users */
	users = (struct irc_user **)hash_values(&server->users);
	for (i = 0; users && users[i]; ++i)
		user_quit(users[i]);
	free(users);

	/* next, cleanup all channels */
	chans = (struct irc_channel **)hash_values(&server->chans);
	for (i = 0; chans && chans[i]; ++i)
		chan_free(chans[i]);
	free(chans);
	hash_free(&server->chans);
	
	/* clean command callback hash table */
	hash_free(&server->commands);

	/* some sort of cleaner for the event hooks */
	myev_server_fini(server);

	/* close socket */
	close(server->sockfd);

	/* free memory */
	free(server);
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

	/* channel user reference structure
	 * channel creator always starts with chanop */
	chan_uref = malloc(sizeof(*chan_uref));
	if (!chan_uref)
	{
		free(new_chan);
		return -1;
	}
	chan_uref->has_chanop = 1;
	chan_uref->user = user;

	/* add user to channel's user table */
	if (hash_insert(&new_chan->users, user->nick, chan_uref))
	{
		free(chan_uref);
		free(new_chan);
		return -1;
	}

	/* add channel to user's chan table */
	if (hash_insert(&user->chans, name, new_chan))
	{
		hash_remove(&new_chan->users, user->nick);
		free(chan_uref);
		free(new_chan);
		return -1;
	}

	/* add channel to server's chan table */
	if (hash_insert(&user->server->chans, name, new_chan))
	{
		hash_remove(&user->chans, name);
		hash_remove(&new_chan->users, user->nick);
		free(chan_uref);
		free(new_chan);
	}
	
	/* channel created succesfully! */
	return 0;
}

/* destroy a channel */
void
chan_free (struct irc_channel *chan)
{
	struct irc_server *server;
	struct chan_user **users;
	int i;

	/* remove chan from server chan table */
	assert((server = chan->server) != NULL);
	assert(hash_remove(&server->chans, chan->name) == chan);

	/* create a simple list of user references */
	users = (struct chan_user **)hash_values(&chan->users);
	for (i = 0; users && users[i]; ++i)
	{
		/* remove our channel from the user's chantable */
		assert(hash_remove(&users[i]->user->chans, chan->name) == chan);
		/* .. and the user from our user table */
		assert(hash_remove(&chan->users, users[i]->user->nick) == users[i]);
		free(users[i]);		/* free chan_user struct */
	}
	free(users);

	/* free/dismantle hash table */
	hash_free(&chan->users);

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
	
	/* attempt to flush buffer */
	return user_flush(user);
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
	char *command, **args, *p, *q;
	int nr_args, i;
	void (*handler)(struct irc_user *, int, char **);
	struct irc_server *server;
	
	/* set time stamp */
	user->time = time(NULL);
	p = user->recvbuf.buffer;
	/* strip newline */
	*(strchrnul(p, '\n')) = 0;
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
	}
	user->recvbuf.index = 0;
	free(args);
}

/* cleanup everything... */
void
user_quit (struct irc_user *user)
{
	struct irc_server *server;
	struct irc_channel **chans;
	struct irc_user **users;
	int i;

	/* some of these cleanup blocks require a user with a nickname */
	if (user->nick[0])
	{
		/* look thru the channels table and remove references to this user from
		 * any channels this user is in, then clear chan table */
		chans = (struct irc_channel **)hash_values(&user->chans);
		for (i = 0; chans && chans[i]; ++i)
			free(hash_remove(&chans[i]->users, user->nick)); /*free(chan_uref)*/
		free(chans);
		hash_free(&user->chans);
	
		/* look thru associated users table and remove refs to this user */
		users = (struct irc_user **)hash_values(&user->assoc_users);
		for (i = 0; users && users[i]; ++i)
			assert(hash_remove(&users[i]->assoc_users, user->nick) == user);
		free(users);
		hash_free(&user->assoc_users);

		/* remove nick from server hash table */
		assert((server = user->server) != NULL);
		assert(hash_remove(&server->users, user->nick) == user);
	}
	
	/* close user's network socket */
	close(user->sockfd);
	
	/* return sendq to pool if present */
	if (user->sendbuf)
		net_sendq_free(user->sendbuf);
	
	/* unset I/O event watcher */
	myev_user_fini(user);

	/* free user structure */
	free(user);
}
