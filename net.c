/* networking layer */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "net.h"

/* pointer to sendQ pool */
static struct sendq *sendq_pool;
static unsigned int sendq_pool_size;

/* fill address structure for ipv4 */
int
net_ipv4_resolve (struct sockaddr_in *address, const char *hostname,
					uint16_t port)
{
	struct hostent *host;
	uint32_t host_ip;

	host = gethostbyname(hostname);
	if (!host)
	{
		fprintf(stderr, "gethostbyname: %s: %s\n", hostname, strerror(errno));
		return -1;
	}
	host_ip = *((uint32_t *)(host->h_addr_list[0]));
	address->sin_family = AF_INET;
	address->sin_port = htons(port);
	address->sin_addr.s_addr = htonl(host_ip);
	return 0;
}

/* get hostname from struct sockaddr_in */
char *
net_name_from_address (struct sockaddr_in *address)
{
	uint32_t ipv4_addr;
	unsigned char a, b, c, d;
	char *ipv4_string;

	ipv4_addr = ntohl(address->sin_addr.s_addr);
	a = (ipv4_addr & 0xff000000) >> 24;
	b = (ipv4_addr & 0x00ff0000) >> 16;
	c = (ipv4_addr & 0x0000ff00) >> 8;
	d = ipv4_addr  & 0x000000ff;
	ipv4_string = malloc(16); /* 16 bytes = '255.255.255.255\0' */
	if (!ipv4_string) return NULL;
	sprintf(ipv4_string, "%u.%u.%u.%u", a, b, c, d);
	return ipv4_string;
}

/* accept a new connection from a listening socket */
int
net_accept (int sockfd, struct sockaddr_in *address)
{
	int ret;
	socklen_t size;

	ret = accept(sockfd, (struct sockaddr *)address, &size);
	return ret;
}

/* create a network socket and connect to hostname:port */
int
net_connect (const char *hostname, uint16_t port)
{
	struct sockaddr_in address;
	int sfd;

	sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sfd == -1) return -1;
	if (net_ipv4_resolve(&address, hostname, port)) return -1;
	if (connect(sfd, (struct sockaddr *)&address, sizeof(address))) return -1;
	return sfd;
}

/* send data from sendQ struct to socket */
ssize_t
net_send (int sfd, struct sendq *buffer)
{
	ssize_t offset;

	/* pass to system call */
	offset = send(sfd, buffer->buffer, buffer->index, 0);
	if (offset == -1)
	{
		perror("send");
		return -1;
	}

	/* relocate leftover buffer contents */
	buffer->index = buffer->index - offset;
	if (buffer->index)
		memmove(buffer->buffer, buffer->buffer + offset, buffer->index);
	return offset;
}

/* receive data from socket to recvQ */
ssize_t
net_recv (int sfd, struct recvq *buffer)
{
	int len, i, j;
	char *buf;

	buf = buffer->buffer;
	i = buffer->index;
	/* pass to system call */
	len = recv(sfd, buf + i, RECVQ_BUF_SZ - i, 0);
	if (len == -1)
	{
		perror("recv");
		return -1;
	}
	/* remove unwanted 0s */
	for (j = 0; j < len; ++j)
	{
		if (!buf[i+j]) buf[i+j] = 1;
	}
	/* update buffer index */
	buffer->index += len;
	i = buffer->index;
	buf[i] = 0;
	return len;
}

/* allocate sendQ buffer from pool */
struct sendq *
net_sendq_alloc ()
{
	struct sendq *ret;

	ret = sendq_pool;
	if (ret)
	{
		/* unlink sendQ unit from pool */
		sendq_pool = ret->next;
		ret->next = NULL;
		/* reset buffer index */
		ret->index = 0;
		sendq_pool_size--;
	} else {
		/* pool ran dry, allocate new sendQ */
		ret = malloc(sizeof(*ret));
		if (!ret) return NULL;
		ret->next = NULL;
		ret->index = 0;
	}
	return ret;
}

/* return sednQ buffer to pool */
void
net_sendq_free (struct sendq *buffer)
{
	if (sendq_pool_size == SENDQ_POOL_MAX_SZ)
	{
		free(buffer);
		return;
	}
	buffer->next = sendq_pool;
	sendq_pool = buffer;
	sendq_pool_size++;
}

/* resize sendQ pool
 * this function looks a bit funny but
 * what it actually does is relatively simple
 * if resize is succesful it returns 0,
 * otherwise if it cannot allocate enough memory
 * it returns -X, -1 if 1 short of target, -2 if
 * 2 short, etc */
/* we dont really need this as net_sendq_alloc()
 * makes new buffers if the pool is empty */
/**** int
net_sendq_resize (unsigned int size)
{
	struct sendq *temp;
	while (size != sendq_pool_size)
	{
		* shrink? *
		if (sendq_pool_size > size)
		{
			temp = net_sendq_alloc();
			free(temp);
		}
		else
		* grow *
		{
			temp = malloc(sizeof(*temp));
			if (!temp)
			{
				* can't grow :( *
				return sendq_pool_size - size;
			}
			net_sendq_free(temp);
		}
	}
	return 0;
} ****/

/* cleanup sendq pool - self explanatory */
static void
_sendq_pool_free ()
{
	struct sendq *temp;

	while (sendq_pool)
	{
		temp = sendq_pool;
		sendq_pool = temp->next;
		free(temp);
		sendq_pool_size--;
	}
}

/* return a socket bound to the ipv4 address hostname:port */
int
net_bind (const char *hostname, uint16_t port)
{
	int sfd, ret, i;
	struct sockaddr_in address;
	struct sendq *temp;

	/* create socket
	 * note SOCK_NONBLOCK, once initialised every operation must
	 * be non blocking for maximum interactivity */
	sfd = socket (AF_INET, SOCK_STREAM || SOCK_NONBLOCK, 0);
	if (sfd == -1)
	{
		perror("socket");
		return -1; /* ..or dy tryn */
	}

	/* setup address structure */
	ret = net_ipv4_resolve(&address, hostname, port);
	if (ret)
	{
		close(sfd);
		return -1;
	}

	/* bind */
	if (bind(sfd, (struct sockaddr *)&address, sizeof(address)) != 0)
	{
		perror("bind");
		close(sfd);
		return -1;
	}
	
	/* set socket as listening */
	if (listen (sfd, SOMAXCONN) != 0)
	{
		perror("listen");
		close(sfd);
		return -1;
	}

	/* now might be a good time to initialise sendQ pool */
	sendq_pool = NULL;
	sendq_pool_size = 0;
	for (i = 0; i < SENDQ_POOL_SZ; i++)
	{
		temp = malloc(sizeof(*temp));
		if (!temp)
		{
			perror("malloc");
			_sendq_pool_free();
			close(sfd);
			return -1;
		}
		temp->next = sendq_pool;
		sendq_pool = temp;
		sendq_pool_size++;
	}

	return sfd;
}
