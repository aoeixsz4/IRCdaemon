#ifndef IRCD_H
#define IRCD_H
/* ircd.h - key structure definitions for the main source file
 * Copyright Joe Doyle 2011 (See COPYING) */
#include <time.h>       /* for time_t */
#include <ev.h>         /* for ev_io */
#include "list.h"       /* for list_t */
#include "irc.h"        /* for IRC_FOO_MAX, etc */

/* non RFC based constants */
/* TODO: find out how much address space is taken if we prealloc
 * enough memory to handle these maximums */
#define IRCD_CLIENTS_MAX    10000
#define IRCD_USERS_MAX      100000
#define IRCD_SERVERS_MAX    100
#define IRCD_CHANS_MAX      500

typedef struct client client_t;
typedef struct user user_t;
typedef struct chan chan_t;
typedef struct server server_t;
typedef struct user_ref user_ref_t;
typedef struct chan_ref chan_ref_t;
#include "net.h"        /* for recv/send_buffer_t */

/* client types */
enum {
    CLIENT_UNREGISTERED = 0,
    CLIENT_USER = 1,
    CLIENT_SERVER = 2
};

/* quit reasons */
enum {
    QUIT_MAX_SENDQ_EXCEEDED = 1,
    QUIT_OUT_OF_MEMORY = 2,
    QUIT_USER_MSG = 3
};

/* struct client represents a connected client
 * list_head is a *next, *prev, providing a doubly linked list
 * ev_io is for libev event handling
 * fd is the file descriptor for the client socket
 * timestamp indicates the time the connection was last active,
 *  if time(NULL) - timestamp > PING_TIMEOUT, drop the connection
 * type indicates whether the client is a server or a user
 * more is a pointer to either a server_t or a user_t */
struct client {
    list_t      list_head;
    ev_io       w;
    int         fd;
    time_t      timestamp;
    int         type;
    void        *more;
    recv_buffer_t   in_buf;
    send_buffer_t   *out_buf;
};

/* struct user represents an IRC user, complete with nick, user, host,
 * channel references, possibly other misc info plus a backref
 * to the relevant client structure (which may be a server type
 * client in the case of remote users) */
struct user {
    list_t      list_head;
    client_t    *client;
    char        nickname[IRC_NICKNAME_MAX+1];
    char        user[IRC_USERNAME_MAX+1];
    char        host[IRC_HOSTNAME_MAX+1];
    list_t      *chans;         /* typdef these to chan_ref_t */
};

/* struct chan represents an IRC channel, the name of the channel
 * and all users who are joined */
struct chan {
    list_t      list_head;
    char        name[IRC_CHANNAME_MAX+1];
    char        topic[IRC_TOPIC_MAX+1];
    list_t      *users;         /* typedef these to user_ref_t */
    list_t      *banmasks;      /* this will be an afterthought */
};

struct server {
    list_t      list_head;
    char        host[IRC_HOSTNAME_MAX+1];
};

/* struct user_ref keep track of what users are in channel X */
struct user_ref {
    list_t      list_head;
    int         modes;          /* for CHANMODE_O */
    user_t      *user;
};

/* struct chan_ref keep track of what channels our user is joined to */
struct chan_ref {
    list_t      list_head;
    chan_t      *chan;
};

#endif /* IRCD_H */
