// Ryan Grayson

#include "list.c" 

int compare(const void *key, const void *with) {
    char *skey = (char*)key;
    char *swith = (char*)with;
    return strcmp(skey, swith);
}

void print_list_item(void *v) {
    char* data = (char*)v;
    printf("%s", data);
}

list_t * insert_input(FILE *file) {
    list_t *linked_list = (list_t*) malloc(sizeof(list_t));
    list_init(linked_list, compare, free);
    char line[42];
    while(fgets(line, 42, file) != NULL) {
        list_insert_tail(linked_list, line);
    }
    return linked_list;
}

void remove_3(list_t *l) {
    int deleted = 0;
    while (l->length > 0 && deleted < 3) {
        list_remove_head(l);
        deleted++;
    }
}

void delete_list(list_t *l) {
    while (l->length > 0) {
        list_remove_head(l);
    }
    free(l->head);
    free(l);
}

void echo(FILE * file) {
    char line[41];
    while(fgets(line, 41, file) != NULL) {
        printf("%s", line);
    }
}

void tail(FILE *file) {
    list_t *linked_list = insert_input(file);
    list_visit_items(linked_list, print_list_item);
    delete_list(linked_list);
}

void tail_remove(FILE * file) {
    list_t *linked_list = insert_input(file);
    while (linked_list->length > 0) {
        remove_3(linked_list);
        list_visit_items(linked_list, print_list_item);
        if (linked_list->length > 0) {
            printf("\n----------------------------------------\n");
        }
        else {
            printf("<EMPTY>");
        }
    }
    delete_list(linked_list);
}

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("ERROR: must supply 2 arguments: file and command\n");
        return 1;
    }
    FILE * file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("ERROR: input file not found\n");
        return 1;
    }
    if (strcmp(argv[2], "echo") == 0) {
        echo(file);
    }
    else if (strcmp(argv[2], "tail") == 0) {
        tail(file);
    }
    else if (strcmp(argv[2], "tail-remove") == 0) {
        tail_remove(file);
    }
    else {
        fclose(file);
        printf("ERROR: invalid command\n");
        return 1;
    }
    fclose(file);
    return 0;
}
