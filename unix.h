#ifndef UNIX_H
#define UNIX_H
/* unix.h header file - Copyright Joe Doyle (See COPYING) */
#include <netinet/in.h>

int unix_listen (const char *, in_port_t);
int unix_connect (const char *, in_port_t);
int unix_accept (int);
int unix_set_nonblock (int);

#endif /* UNIX_H */
