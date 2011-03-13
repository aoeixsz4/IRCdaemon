#ifndef __NET_H__
#define __NET_H__

/* i think we technically need this for struct sockaddr_in */
#include <netinet/in.h>

/* buffer size constants */
#define SENDQ_BUF_SZ (4096 * 8)
#define RECVQ_BUF_SZ (4096 * 2)

/* initial sendq pool size, actual pool size may increase with high load */
#define SENDQ_POOL_SZ 16
#define SENDQ_POOL_MAX_SZ 256

struct sendq {
	void *next; /* pointer to struct sendq, for use in the sendq pool */
	unsigned int index; /* buffer[index] is the next unused byte */
	char buffer[SENDQ_BUF_SZ];
};

struct recvq {
	unsigned int index; /* buffer[index] is the next unused byte */
	char buffer[RECVQ_BUF_SZ];
};

int net_bind (const char *, uint16_t);
int net_ipv4_resolve (struct sockaddr_in *, const char *, uint16_t);
struct sendq *net_sendq_alloc (void);
void net_sendq_free (struct sendq *);
ssize_t net_send (int, struct sendq *);
ssize_t net_recv (int, struct recvq *);
int net_accept (int, struct sockaddr_in *);
char *net_name_from_address (struct sockaddr_in *);

/* this may never serve any useful function, unless I actually implement
 * server linkage, however, I will include it for .. completeness */
int net_connect (const char *, uint16_t);

#endif /* __NET_H__ */
