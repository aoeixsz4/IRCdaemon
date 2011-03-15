#ifndef __IRC_H__
#define __IRC_H__

#include "core.h"		/* struct irc_user * */

#define IRC_DEFAULT_PORT 6667
#define IRC_HOST "0.0.0.0"
#define IRC_SERVER_NAME "ircdaemon.localhost.net"

/* message codes */
enum {
	IRC_WELCOME			= 1,
	IRC_MOTD_PIECE		= 372,
	IRC_MOTD_BEGIN		= 375,
	IRC_MOTD_END		= 376,
	IRC_ERROR_NOORIG	= 409,
	IRC_ERROR_404		= 421,
	IRC_ERROR_BADNICK	= 432,
	IRC_ERROR_NICKINUSE	= 433,
	IRC_ERROR_NOREG		= 451,
	IRC_ERROR_FEWPARAM	= 461,
	IRC_ERROR_ISREG		= 462
};

/* necessary? */
#define WELCOME_MESSAGE "Welcome to ircdaemon.localhost.net!"

/* charsets (for BADNICK error etc) */
#define ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALPHANUM ALPHA "0123456789"
#define ALPHAPLUS ALPHA "_`|"
#define ALPHANUMPLUS ALPHANUM "_`|-"

void irc_cmd_nick(struct irc_user *, int, char **);
void irc_cmd_user(struct irc_user *, int, char **);
void irc_cmd_ping(struct irc_user *, int, char **);
void irc_cmd_pong(struct irc_user *, int, char **);
void irc_cmd_motd(struct irc_user *, int, char **);

#endif /* __IRC_H__ */
