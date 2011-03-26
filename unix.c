/* unix.c - Copyright Joe Doyle 2011 (See COPYING)
 * Unix specific socket and pipe handling */
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
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

/* resolve takes a pointer to a sockaddr_in struct as well as a hostname
 * and portnumber and fills the structure with network address data
 * returns -1 on error, 0 on success */
static int
resolve (const char *host, in_port_t port, struct sockaddr_in *address)
{
    struct hostent *host_p;

    host_p = gethostbyname(host);
    if (!host_p)
    {
        fprintf(stderr, "Resolution of %s failed: %s\n",
                                    host, hstrerror(h_errno));
        return -1;
    }
    /* unix socket code is filthy
     * BUGFIX: previously assumed h_addr_list[0] is host endianness
     * in actual fact it is already in network byte order */
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = *((in_addr_t *)host_p->h_addr_list[0]);
    return 0;
}

/* unix_connect takes host name and port number and attempts to
 * initiate a socket connection to a listener at the given address */
int
unix_connect (const char *host, in_port_t port)
{
    int fd, ret;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;
    /*if (unix_set_nonblock(fd))
    {
        close(fd);
        return -1;
    }*/
    if (resolve(host, port, &addr))
    {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        fprintf(stderr, "Connect to %s:%d failed: %s\n",
                               host, port, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/* unix_listen takes a host name and port number and attempts to bind a
 * socket to the address and tell the network stack to listen for
 * connections, simple! If successful, the bound socket descriptor
 * is returned, otherwise -1 */
int
unix_listen (const char *host, in_port_t port)
{
    int fd, ret;
    struct sockaddr_in sockaddr;

    /* NB: all sockets will be set to O_NONBLOCK here in unix.c as
     * I've not found anything in the glib GIOChannel docs regarding
     * blocking vs non blocking operations */
    fd = socket(AF_INET, SOCK_STREAM, 0);
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
    /* resolve address into weird sockaddr_in structure */
    ret = resolve(host, port, &sockaddr);
    if (ret == -1)
    {
        close(fd);
        return -1;
    }
    ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (ret == -1)
    {
        fprintf(stderr, "Could not bind %s:%d: %s\n",
                            host, port, strerror(errno));
        close(fd);
        return -1;
    }
    ret = listen(fd, SOMAXCONN);
    if (ret == -1)
    {
        fprintf(stderr, "Bound %s:%d but listen failed: %s\n",
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
