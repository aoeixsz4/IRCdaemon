#ifndef __IRC_H__
#define __IRC_H__

/* we need headers for hash_table, sockaddr, sendq and ev_io */
#include <netinet/in.h>
#include <ev.h>
#include <time.h>
#include "net.h"
#include "hash.h"

#define IRC_DEFAULT_PORT 6667
#define IRC_HOST "0.0.0.0"
#define IRC_SERVER_NAME "ircdaemon.localhost.net"

/* arbitrary array size limits */
#define CHAN_MAX_TOPIC 256
#define CHAN_MAX_NAME 32
#define CHAN_MAX_MODES 64
#define CHAN_MAX_BANS 64
#define USER_MAX_NICK 16
#define USER_MAX_IDENT 16
#define USER_MAX_RNAME 64
#define USER_MAX_HOST 64

/* message codes */
enum {
	IRC_WELCOME			= 1,
	IRC_MOTD_PIECE		= 372,
	IRC_MOTD_BEGIN		= 375,
	IRC_MOTD_END		= 376,
	IRC_ERROR_NOORIG	= 409,
	IRC_ERROR_404		= 421,
	IRC_ERROR_BADNICK	= 432,
	IRC_ERROR_FEWPARAM	= 461
};

/* necessary? */
#define WELCOME_MESSAGE "Welcome to ircdaemon.localhost.net!"

/* charsets (for BADNICK error etc) */
#define ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALPHANUM ALPHA "0123456789"
#define ALPHAPLUS ALPHA "_`|"
#define ALPHANUMPLUS ALPHANUM "_`|-"

/* global irc server state structure */
struct irc_server {
	int sockfd; /* what do you think? */
	struct hash_table users;
	struct hash_table chans;
	struct hash_table commands;
	struct ev_io *ev_w;
};

struct chan_user {
	int has_chanop;
	struct irc_user *user;
};

struct irc_channel {
	char name[CHAN_MAX_NAME];
	char modes[CHAN_MAX_MODES];
	char topic[CHAN_MAX_TOPIC];
	char *bans[CHAN_MAX_BANS];

	/* entries in this table point to a wrapper struct chan_user, with a
	 * boolean for chanop status */
	struct hash_table users;
	struct irc_server *server;
};

struct irc_user {
	/* these are pretty self-explanatory */
	char nick[USER_MAX_NICK];
	char ident[USER_MAX_IDENT];
	char host[USER_MAX_HOST];

	/* channels the user is linked to */
	struct hash_table chans;
	struct hash_table assoc_users;

	/* networking stuff */
	int sockfd;
	struct ev_io *ev_w;	 /* event watcher */
	struct recvq  recvbuf; /* dedicated */
	struct sendq *sendbuf; /* assigned on an as-needed basis */

	/* timestamp */
	time_t time;

	/* quit flag */
	int quit;

	/* backref to global server state struct each event watcher is
	 * given a pointer to a struct irc_user, but the callbacks
	 * also need the global state */
	struct irc_server *server;
};

void user_init(int, struct sockaddr_in *, struct irc_server *);
void user_quit(struct irc_user *);
void user_free(struct irc_user *);
void user_read(struct irc_user *);
ssize_t user_send(struct irc_user *, const char *);
ssize_t user_sendl(struct irc_user *, const char *);
ssize_t user_sendf(struct irc_user *, const char *, ...);
ssize_t user_flush(struct irc_user *);
struct irc_server *irc_server_init(void);
void irc_server_cleanup(struct irc_server *);
int chan_init(const char *, struct irc_user *);
void chan_free(struct irc_channel *);
ssize_t user_sendl(struct irc_user *, const char *);

#endif /* __IRC_H__ */
