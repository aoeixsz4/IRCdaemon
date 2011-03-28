/* net.c - networking layer, note that unix specific network handling
 * is found in unix.c. Copyright Joe Doyle 2011 (See COPYING) */
#include <sys/socket.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "net.h"
#include "ircd.h"
#include "list.h"
#include "irc.h"

static send_buffer_t *send_buffer_pool = NULL;
static int pool_size = 0;

/* net_free_sendbuf(buffer)
 * puts a send buffer back on the queue, unless
 * pool_size > MAX_POOL_SIZE, in which case the buffer is given
 * to free() */
void
net_free_sendbuf (send_buffer_t *buffer)
{
    if (pool_size == MAX_POOL_SIZE)
        free(buffer);
    else
    {
        list_push((list_t **)&send_buffer_pool, (list_t *)buffer);
        pool_size++;
    }
}

/* net_alloc_sendbuf(void)
 * pops a send buffer off the pool, malloc()s one if the pool is exhausted */
send_buffer_t *
net_alloc_sendbuf (void)
{
    send_buffer_t *buffer;

    if (pool_size)
    {
        buffer = (send_buffer_t *)list_pop((list_t **)&send_buffer_pool);
        pool_size--;
    }
    else
        buffer = malloc(sizeof(*buffer));
    return buffer;
}

/* net_flush(client)
 * attempts to send any data in client->out_buf, if there is any */
ssize_t
net_flush (client_t *client)
{
    ssize_t bytes_sent = 0;

    if (client->out_buf)
    {
        bytes_sent = recv(client->fd, client->out_buf->buffer,
                            client->out_buf->index, 0);
        if (bytes_sent <= 0) /* faaaail */
        {
            if (bytes_sent == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK))
                drop(client, errno);    /* unexpected error :O */
            else
                bytes_sent = 0;         /* nothing sent */
        }
        else if (bytes_sent < client->out_buf->index)
        {
            /* move unsent data to start of buffer */
            client->out_buf->index -= bytes_sent;
            memmove(client->out_buf->buffer,
                    client->out_buf->buffer + bytes_sent,
                    client->out_buf->index);
        }
        else
        {
            /* bytes_sent must be == index */
            net_free_sendbuf(client->out_buf);
            client->out_buf = NULL;
        }
    }
    return bytes_sent;
}

/* net_send(client, message, size)
 * send a message, size bytes long, to the client. if send() fails beacuse it
 * would block, tack the message on the client's send buffer for a later write
 * NB: if this returns -1, client no longer points to valid memory! */
ssize_t
net_send (client_t *client, const char *message, ssize_t size)
{
    ssize_t bytes_sent;
    if (client->out_buf)
    {
        /* append message to out buffer */
        if (client->out_buf->index + size > BUFFER_SIZE)
        {
            /* looks like the connection isnt reading from us,
             * or lagging too much or something */
            drop(client, QUIT_MAX_SENDQ_EXCEEDED);
            return -1;
        }
        memcpy(client->out_buf->buffer + client->out_buf->index,
                message, size);
        client->out_buf->index += size;
        return net_flush(client);
    }
    /* send() data, this may fail innocuously if it would block, in which
     * case queue the message on a send buffer to be flushed when libev
     * detects writeability */
    bytes_sent = send(client->fd, message, size, 0);
    if (bytes_sent <= 0)
    {
        /* set this to 0 so line 56 copies the right number of bytes */
        bytes_sent = 0;
        if (!(errno == EAGAIN || errno == EWOULDBLOCK))
        {
            /* unexpected error... close connection */
            drop(client, errno);
            return -1;
        }
    }   
    if (bytes_sent != size)
    {
        /* queue unsent data on a send buffer */
        client->out_buf = (send_buffer_t *)net_alloc_sendbuf();
        if (!client->out_buf)
        {  
            /* this probably means we're out of memory, we can mitigate
             * this problem by dropping the client */
            drop(client, QUIT_OUT_OF_MEMORY);
            return -1;
        }
        client->out_buf->index = 0;
        memcpy(client->out_buf->buffer + client->out_buf->index,
                message + bytes_sent, size - bytes_sent);
        client->out_buf->index += size - bytes_sent;
    }
    return size;
}

/* net_manysendvf(client_t **clients, fmt, ap)
 * send format string to a number of clients */
ssize_t
net_manysendvf (client_t **clients, const char *fmt, va_list ap)
{
    char text[IRC_MESSAGE_MAX + 1];
    int i;
    ssize_t size, ret, ret2;
    
    /* make string from format */
    size = vsnprintf(text, IRC_MESSAGE_MAX, fmt, ap);

    /* add \r\n to the end of the message */
    if (size > IRC_MESSAGE_MAX - 2)
        size = IRC_MESSAGE_MAX - 2;
    strcpy(text + size, "\r\n");
    size += 2;

    /* for each client, do send */
    ret = 0;
    for (i = 0; clients[i]; ++i)
    {
        ret2 = net_send(clients[i], text, size);
        /* if any send returns an error, then we return an error */
        if (ret != -1)
            ret = ret2;
    }
    return ret;
}

/* net_manysendf(client_t **clients, fmt, ...)
 * wrapper for net_manysendvf() */
ssize_t
net_manysendf (client_t **clients, const char *fmt, ...)
{
    va_list ap;
    ssize_t ret;

    va_start(ap, fmt);
    ret = net_manysendvf(clients, fmt, ap);
    va_end(ap);
    return ret;
}

/* net_sendvf(*client, fmt, ap)
 * send format string to a client */
ssize_t
net_sendvf (client_t *client, const char *fmt, va_list ap)
{
    char text[IRC_MESSAGE_MAX + 1];
    ssize_t size;

    /* make format string */
    size = vsnprintf(text, IRC_MESSAGE_MAX, fmt, ap);

    /* add \r\n to end of message */
    if (size > IRC_MESSAGE_MAX - 2)
        size = IRC_MESSAGE_MAX - 2;
    strcpy(text + size, "\r\n");
    size += 2;

    /* do send */
    return net_send(client, text, size);
}

/* net_sendf(*client, fmt, ...)
 * wrapper for net_sendvf() */
ssize_t
net_sendf (client_t *client, const char *fmt, ...)
{
    va_list ap;
    ssize_t ret;

    va_start(ap, fmt);
    ret = net_sendvf(client, fmt, ap);
    va_end(ap);
    return ret;
}
