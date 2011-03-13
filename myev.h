/* libev wrappers header file */
#ifndef __MYEV_H__
#define __MYEV_H__

/* needed for struct irc_user and struct irc_server */
#include <ev.h>
#include "irc.h"

/* event loop initialiser MUST BE RUN FIRST */
void myev_init(void);

/* register watchers */
int myev_server_init(struct irc_server *);
void myev_server_fini(struct irc_server *);
int myev_user_init(struct irc_user *);
void myev_user_fini(struct irc_user *);
int myev_timer_init(struct irc_server *);

/* set EV_READ | EV_WRITE or just EV_READ on a watcher */
void myev_set_rw(struct irc_user *);
void myev_set_ro(struct irc_user *);

/* enter event loop */
void myev_run(void);

#endif /* __MYEV_H__ */
