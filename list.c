/* list.c - linked list handling functions
 * Copyright Joe Doyle (See COPYING) */
#include <stdlib.h>
#include "list.h"

list_t *
list_pop (list_t **n0)
{
    list_t *ret;

    ret = *n0;
    if (*n0)
    {
        *n0 = (*n0)->next;
        (*n0)->prev = NULL;
        ret->next = NULL;
    }
    return ret;
}

void
list_push (list_t **n0, list_t *new)
{
    if (*n0)
        (*n0)->prev = new;
    new->next = *n0;
    new->prev = NULL;
    *n0 = new;
}

void
list_unlink (list_t *link)
{
    if (link->prev)
        link->prev->next = link->next;
    if (link->next)
        link->next->prev = link->prev;
}
