/* unix.c - Copyright Joe Doyle 2011 (See COPYING)
 * Unix specific socket and pipe handling */
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
extern int h_errno;

/* unix_set_nonblock sets the nonblocking flag on an open file
 * descriptor */
int
unix_set_nonblock (int fd)
{
    int flags, ret;

    flags = fcntl(fd, F_GETFL);
    if (flags == -1) ret = -1;
    else 
        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1) ret = -1;
    else if (ret != -1) ret = 0;
    if (ret == -1)
        fprintf(stderr, "fcntl(): %s\n", strerror(errno));
    return ret;
}

/* unix_listen takes a host name and port number and attempts to bind a
 * socket to the address and tell the network stack to listen for
 * connections, simple! If successful, the bound socket descriptor
 * is returned, otherwise -1 */
int
unix_listen (const char *host, in_port_t port)
{
    int fd, fdfl, ret;
    struct sockaddr_in sockaddr;
    struct hostent *host_p;
    in_addr_t ip_addr;

    /* NB: all sockets will be set to O_NONBLOCK here in unix.c as
     * I've not found anything in the glib GIOChannel docs regarding
     * blocking vs non blocking operations */
    fd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);
    if (fd == -1)
        return -1;
    /* NB: non blocking sockets are essential otherwise we might
     * block on a read() or write() involving one laggy client
     * while all the others wait */
    ret = unix_set_nonblock(fd);
    if (ret == -1)
    {
        close(fd);
        return -1;
    }
    host_p = gethostbyname(host);
    if (!host_p)
    {
        fprintf(stderr, "Resolution of %s failed: %s\n",
                                    host, hstrerror(h_errno));
        close(fd);
        return -1;
    }
    /* ip_addr is host-endianness */
    /* unix socket code is filthy */
    ip_addr = *((in_addr_t *)host_p->h_addr_list[0]);
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = htonl(ip_addr);
    ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (ret == -1)
    {
        fprintf(stderr, "Could not bind %s:%s: %s\n",
                            host, port, strerror(errno));
        close(fd);
        return -1;
    }
    ret = listen(fd, SOMAXCONN);
    if (ret == -1)
    {
        fprintf(stderr, "Bound %s:%s but listen failed: %s\n",
                            host, port, strerror(errno));
        close(fd);
        return -1;
    }
    /* success!! */
    return fd;
}

/* unix_accept() creates and returns a new socket for an incoming
 * client connection */
int
unix_accept (int listenfd)
{
    socklen_t length;
    struct sockaddr address;
    int fd;

    length = sizeof(address);
    fd = accept(listenfd, &address, &length);
    if (fd == -1)
        fprintf(stderr, "Accept failed: %s\n", strerror(errno));
    else if (unix_set_nonblock(fd))
    {
        close(fd);
        fd = -1;
    }
    return fd;
}
