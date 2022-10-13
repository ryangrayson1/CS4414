// Ryan Grayson

#ifndef list
#define list

typedef struct list_item {
    struct list_item *pred, *next;
    void *datum;
} list_item_t;

typedef struct list {
    list_item_t *head, *tail;
    unsigned length;
    int (*compare) (const void *key, const void *with);
    void (*datum_delete) (void *);
} list_t;

#endif
