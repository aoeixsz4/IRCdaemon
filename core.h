#ifndef __CORE_H__
#define __CORE_H__

/* we need headers for hash_table, sockaddr, sendq and ev_io */
#include <netinet/in.h>
#include <ev.h>
#include <time.h>
#include "net.h"
#include "hash.h"

/* arbitrary array size limits */
#define CHAN_MAX_TOPIC 256
#define CHAN_MAX_NAME 32
#define CHAN_MAX_MODES 64
#define CHAN_MAX_BANS 64
#define USER_MAX_NICK 16
#define USER_MAX_IDENT 16
#define USER_MAX_RNAME 64
#define USER_MAX_HOST 64


/* global irc server state structure */
struct irc_server {
	int sockfd; /* what do you think? */
	struct hash_table users;
	struct hash_table chans;
	struct hash_table commands;
	struct ev_io *ev_w;
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

	/* give each channel a refcount so uc_unlink() can call chan_free()
	 * when the refcount becomes zero */
	int refcount;
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

struct chan_user {
	int has_chanop;
	struct irc_user *user;
};

/* helps implement user<->user association
 * refcount is vital as users may be associated
 * via a number of channels.
 * PART will have a hard time figuring out whether
 * or not to unlink 2 users, with the refcount it
 * need only uu_unlink(user_a, user_b) which
 * will decrement the refcount then delink properly if 0 */
struct user_user {
	int refcount;
	struct irc_user *user;
};

struct user_user *uu_link(struct irc_user *, struct irc_user *);
int uu_update(struct irc_user *, const char *, struct irc_user *);
void uu_unlink(struct irc_user *, struct irc_user *);
struct chan_user *uc_link(struct irc_user *, struct irc_channel *);
int uc_update(struct irc_user *, const char *, struct irc_channel *);
void uc_unlink(struct irc_user *, struct irc_channel *);
void user_init(int, struct sockaddr_in *, struct irc_server *);
void user_quit(struct irc_user *, const char *);
void user_free(struct irc_user *);
void user_read(struct irc_user *);
ssize_t user_send(struct irc_user *, const char *);
ssize_t user_sendl(struct irc_user *, const char *);
ssize_t user_sendf(struct irc_user *, const char *, ...);
ssize_t user_flush(struct irc_user *);
struct irc_server *server_init(void);
void server_shutdown(struct irc_server *);
int chan_init(const char *, struct irc_user *);
void chan_free(struct irc_channel *);
ssize_t user_sendl(struct irc_user *, const char *);

#endif /* __CORE_H__ */
