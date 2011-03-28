#ifndef LIST_H
#define LIST_H
/* list.h - linked list structure and prototype definitions
 * Copyright Joe Doyle 2011 (See COPYING) */

typedef struct list list_t;

struct list {
    list_t  *next;
    list_t  *prev;
};

list_t *list_pop (list_t **);
void list_push (list_t **, list_t *);
void list_unlink (list_t **, list_t *);
#endif /* LIST_H */
