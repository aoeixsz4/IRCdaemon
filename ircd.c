/* ircd.c - Copyright Joe Doyle (See COPYING)
 * this is the main ircd source file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <ev.h>         /* requires libev */
#include "unix.h"       /* for unix_listen/accept */
#include "ircd.h"       /* essential data structure definitions */

#define ANY "0.0.0.0"
#define IRCD_HOST ANY
#define IRCD_PORT 6667

static int          server_fd;
static ev_io        server_w;
static ev_signal    sigterm_w;

static client_t     *client_list = NULL;
static int          nr_clients = 0;
static user_t       *user_list = NULL;
static int          nr_users = 0;
static chan_t       *chan_list = NULL;
static int          nr_chans = 0;
static server_t     *server_list = NULL;
static int          nr_servers = 0;

static void
client_cb (EV_P_ ev_io *w, int revents)
{
    
}
    
static void
server_cb (EV_P_ ev_io *w, int revents)
{
    int new_fd;
    client_t *my_client;

    /* NOTES: possible event bits are EV_READ and EV_ERROR
     * however, EV_ERROR shouldn't really happen, so if (EV_ERROR) fatal
     * otherwise assume EV_READ */
    if (revents & EV_ERROR)
    {
        fprintf(stderr, "FATAL: unexpected EV_ERROR on server event watcher\n");
        exit(1);
    }

    /* assume EV_READ */
    new_fd = unix_accept(server_fd);
    if (new_fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
        return;     /* nothing interesting happened */
    else if (new_fd == -1)
    {
        fprintf(stderr, "accept failed unexpectedly: %s\n", strerror(errno));
        return;
    }
    
    fprintf(stderr, "accepted connection: fd: %d\n", new_fd);

    /* register client connection in state */
    if (nr_clients <= IRCD_CLIENTS_MAX
        && (my_client = malloc(sizeof(*my_client))))
    {
        list_push(&client_list, my_client);
        nr_clients++;

        /* save client info and init watcher */
        my_client->fd = new_fd;
        my_client->timestamp = time(NULL);
        ev_io_init(EV_A_ &my_client->w, &client_cb, new_fd, EV_READ);
        ev_io_start(EV_A_ &my_client->w);
        my_client->w.data = my_client; /* lol recursion */
        return;
    }
    else if (my_client)
        /* we hit max clients?? */
        fprintf(stderr, "too many clients, dropping connection %d\n", new_fd);
    else
        fprintf(stderr, "malloc: %s\n", strerror(errno));
    close(new_fd);
    free(my_client);
}

static void
sigterm_cb (EV_P_ ev_signal *w, int revents)
{
    fprintf(stderr, "received signal %d: breaking event loop\n", revents);
    ev_break(EV_A_ EVBREAK_ALL);
}

static void
ircd ()
{
    FILE *logfile;
    struct ev_loop *loop;

    /* reopn stderr to log to ircd.err */
    logfile = freopen("ircd.err", "w", stderr);
    if (logfile == NULL)
    {
        fprintf(stderr, "freopen: ircd.err: %s\n", strerror(errno));
        fprintf(stderr, "logging to stderr instead\n");
    }

    /* bind/listen server socket */
    server_fd = unix_listen(IRCD_HOST, IRCD_PORT);
    if (server_fd == -1)
    {
        fprintf(stderr, "Could not register socket, exiting.");
        exit(1);
    }

    /* init libev default loop */
    loop = ev_default_loop(0);

    /* set up libev callback for incoming connections */
    ev_io_init(EV_A_ &server_w, &server_cb, server_fd, EV_READ);
    ev_io_start(EV_A_ &server_w);
    
    /* ignore some signals, catch TERM with our own handler, consider using
     * libev for the SIGTERM callback for consistency's sake */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    ev_signal_init(EV_A_ &sigterm_w, &sigterm_cb, SIGTERM);
    ev_signal_start(EV_A_ &sigterm_w);

    /* enter main loop */
    ev_run(EV_A_ 0);

    /* clean server state here, in case later on I decide I want to handle
     * SIGHUP to reboot the server without exiting the process or something */
    ev_io_stop(EV_A_ &server_w);
    
    /* close socket */
    close(server_fd);
}

int
main (int argc, char **argv, char **envp)
{
    switch (fork())
    {
        case -1:    fprintf(stderr, "Fork: %s\n", strerror(errno));
                    return 1;
        case 0:     ircd();
        default:    break;
    }
    return 0;
}
