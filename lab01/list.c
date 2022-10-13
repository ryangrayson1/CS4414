// Ryan Grayson

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

void list_init(list_t *l, int (*compare) (const void *key, const void *with), void (*datum_delete) (void *datum)) {
    l->head = (list_item_t*) malloc(sizeof(list_item_t));
    l->head->next = NULL;
    l->tail = l->head;
    l->length = 0;
    l->compare = compare;
    l->datum_delete = datum_delete;
}

void list_visit_items(list_t *l, void (*visitor) (void *v)) {
    list_item_t * cur = l->head->next;
    while (cur != NULL) {
        visitor(cur->datum);
        cur = cur->next;
    }
}

void list_insert_tail(list_t *l, void *v) {
    list_item_t *new_item = (list_item_t*) malloc(sizeof(list_item_t));
    new_item->datum = (void*) malloc(sizeof(char) * 42);
    strcpy(new_item->datum, v);
    new_item->next = NULL;
    new_item->pred = l->tail;
    l->tail->next = new_item;
    l->tail = new_item;
    l->length++;
}

void list_remove_head(list_t *l) {
    if (l->length == 0) {
        printf("ERROR - cannot remove from empty list\n");
    }
    else if (l->length == 1) {
        free(l->tail->datum);
        free(l->tail);
        l->head->next = NULL;
        l->tail = l->head;
        l->length = 0;
    }
    else {
        list_item_t *temp = l->head->next->next;
        free(l->head->next->datum);
        free(l->head->next);
        l->head->next = temp;
        temp->pred = l->head;
        l->length--;
    }
}
