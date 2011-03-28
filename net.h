#ifndef NET_H
#define NET_H
/* net.h - header file for network layer
 * Copyright Joe Doyle 2011 (See COPYING) */
#include "list.h"

#define BUFFER_SIZE (4 * 4096)
#define MAX_POOL_SIZE (IRCD_CLIENTS_MAX / 10)

typedef struct recv_buffer recv_buffer_t;
typedef struct send_buffer send_buffer_t;

/* message buffers */
struct recv_buffer {
    int     index;
    char    buffer[BUFFER_SIZE];
};

struct send_buffer {
    list_t  list_head;
    int     index;
    char    buffer[BUFFER_SIZE];
};

/* moved include "ircd.h" down here because ircd.h requires
 * the above types */
#include "ircd.h"
ssize_t net_send (client_t *, const char *, ssize_t);
ssize_t net_manysendvf (client_t **, const char *, va_list);
ssize_t net_manysendf (client_t **, const char *, ...);
ssize_t net_sendvf (client_t *, const char *, va_list);
ssize_t net_sendf (client_t *, const char *, ...);
send_buffer_t *net_alloc_sendbuf (void);
void net_free_sendbuf (send_buffer_t *);
#endif /* NET_H */
