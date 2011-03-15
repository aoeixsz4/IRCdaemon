/* irc.c - irc protocol command callbacks */
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

/* JOIN */
void
irc_join (struct irc_user *user, int nr_args, char **args)
{
	char *p, *q;
	struct irc_channel **chans;
	struct irc_user **users;
	int i;

	/* for starters, make sure user is registered */
	if (!(user->nick[0] && user->ident[0]))
	{
		user_sendf(user, ":%s %3d :You have not registered\n",
					IRC_SERVER_NAME, IRC_ERROR_NOREG);
		user_flush(user);
		return;
	}

	/* check nr_args */
	if (nr_args < 1)
	{
		user_sendf(user, ":%s %3d JOIN :Not enough parameters\n",
					IRC_SERVER_NAME, IRC_ERROR_FEWPARAM);
		user_flush(user);
		return;
	}

	/* split targets by comma */
	p = args[0];
	while (q = strtok(p, ','))
	{
		/* only the first call to strtok() should be passed the string pointer
		 * subequent calls take NULL */
		p = NULL;
		
		/* JOIN 0 is a special case, instructing us to remove the user from
		 * all channels */
		if (*q == '0')
		{
			chans = (struct irc_channel **)hash_values(&user->chans);
			for (i = 0; chans && chans[i]; ++i)
			{
				/* Here we need to report PART messages for each chan
				 * fortunately we can do _part(user, chans[i]->name)
				 * TODO _part() */
			}
			free(chans);
		}

		/* More stuff to do......
		 * ......................
		 * ...................... */
	}
}

/* PING */
void
irc_ping (struct irc_user *user, int nr_args, char **args)
{
	char *origin;
	if (!nr_args)
	{
		user_sendf(user, ":%s %3d %s :No origin specified\n",
					IRC_SERVER_NAME, IRC_ERROR_NOORIG, user->nick);
		user_flush(user);
	}
	else
	{
		origin = args[0];
		if (origin[0] == ':') origin++;
		user_sendf(user, ":%s PONG %s :%s\n",
					IRC_SERVER_NAME, IRC_SERVER_NAME, origin);
		user_flush(user);
	}
}

/* PONG */
void
irc_pong (struct irc_user *user, int nr_args, char **args)
{
	if (!nr_args)
	{
		user_sendf(user, ":%s %3d %s :No origin specified\n",
					IRC_SERVER_NAME, IRC_ERROR_NOORIG, user->nick);
		user_flush(user);
	}
	/* else NOOP */
}

/* MOTD */
void
irc_motd (struct irc_user *user, int nr_args, char **args)
{
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_BEGIN,
				user->nick, "- ircdaemon message of the day -");
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_PIECE,
				user->nick, "- bla bla bla -");
	user_sendf(user, ":%s %3d %s :%s\n", IRC_SERVER_NAME, IRC_MOTD_END,
				user->nick, "- enjoy your stay! -");
	user_flush(user);
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
void
irc_user (struct irc_user *user, int nr_args, char **args)
{
	char *ident;
	int i, reg;
	if (nr_args < 4)
	{
		user_sendf(user, ":%s %3d %s :Not enough parameters\n",
					IRC_SERVER_NAME, IRC_ERROR_FEWPARAM, user->nick);
		user_flush(user);
		return;
	}

	/* users can only register once! */
	if (user->ident[0])
	{
		user_sendf(user, ":%s %3d :You may not reregister\n",
					IRC_SERVER_NAME, IRC_ERROR_ISREG);
		user_flush(user);
		return;
	}

	ident = args[0];
	for (i = 0; ident[i]; ++i)
	{
		if (!strchr(ALPHANUMPLUS, ident[i]))
		{
			user_sendf(user, "ERROR :Closing Link: %s[%s] (NAUGHTY!)\n",
						user->nick, user->host);
			user_flush(user);
			user_quit(user);
		}
	}
	/* check registration status */
	if (user->nick[0])
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
void
irc_nick (struct irc_user *user, int nr_args, char **args)
{
	char *nick, *oldnick;
	int i, reg;
	struct irc_server *server;
	struct irc_user **users;
	struct chan_user *chan_uref;

	if (!nr_args)
	{
		user_sendf(user, ":%s %3d NICK :Not enough parameters\n",
					IRC_SERVER_NAME, IRC_ERROR_FEWPARAM);
		user_flush(user);
		return;
	}
	nick = args[0];
	if (!strchr(ALPHAPLUS, nick[0]))
	{
		user_sendf(user, ":%s %3d %s :Erroneous nickname\n",
					IRC_SERVER_NAME, IRC_ERROR_BADNICK, nick);
		user_flush(user);
		return;
	}
	for (i = 1; nick[i]; ++i)
	{
		if (!strchr(ALPHANUMPLUS, nick[i]))
		{
			user_sendf(user, ":%s %3d %s :Erroneous nickaname\n",
						IRC_SERVER_NAME, IRC_ERROR_BADNICK, nick);
			user_flush(user);
			return;
		}
	}

	/* check if nickname is already in use */
	server = user->server;
	if (hash_lookup(&server->users, nick))
	{
		user_sendf(user, ":%s %3d %s :Nickname is already in use\n",
					IRC_SERVER_NAME, IRC_ERROR_NICKINUSE, nick);
		user_flush(user);
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
	oldnick = NULL;
	if (user->nick[0])
	{
		hash_remove(&server->users, user->nick);
		oldnick = strdup(user->nick);
		if (!oldnick)
		{
			user_quit(user);
			return;
		}
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
	else if (user->ident[0] && oldnick)
	{
		/* confirm nick change */
		user_sendf(user, ":%s!%s@%s NICK :%s\n", oldnick, user->ident,
					user->host, user->nick);
		user_flush(user);

		/* report nick change to associated users
		 * and not forgetting to update the association! */
		users = (struct irc_user **)hash_values(&user->assoc_users);
		for (i = 0; users && users[i]; ++i)
		{
			user_sendf(users[i], ":%s!%s@%s NICK :%s\n", oldnick,
						user->ident, user->host, user->nick);
			user_flush(users[i]);
			assert(hash_remove(&users[i]->assoc_users, oldnick) == user);
			/* is there anything we can usefully do if this fails?
			 * TODO we need refcounts on user linkage, which means an
			 * intermediate struct user_uref { int refcount;
			 * .. struct irc_user *user; }; */
			if (hash_insert(&users[i]->assoc_users, user->nick, user))
			{
				assert(hash_remove(&user->assoc_users, users[i]->nick)
						== users[i]);
			}
		}
		free(users);

		/* update nick to channels too */
		chans = (struct irc_channel **)hash_values(&user->chans);
		for (i = 0; chans && chans[i]; ++i)
		{
			chan_uref = hash_remove(&chans[i]->users, oldnick);
			assert(chan_uref && chan_uref->user == user);
			/* again, how best to handle failure here..
			 * if this DOES fail, (out of memory most likely)
			 * message sent to the channel will no longer be sent
			 * to our user, perhaps send a PART message...?
			 * we definitely must remove this channel from the
			 * user's channel table in the fail situation though
			 * or our assert() (the one above) will cause a crash!! */
			if (hash_insert(&chans[i]->users, chan_uref))
			{
				assert(hash_remove(&user->chans, chans[i]->name) == chans[i]);
			}
		}
		free(chans);
	}
	free(oldnick);
}
