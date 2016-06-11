//
//  linked_list.h
//  ChatServer
//
//  Created by Patrick Mintram on 30/05/2016.
//  Copyright © 2016 Patrick Mintram. All rights reserved.
//

#ifndef linked_list_h
#define linked_list_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define KILL(a)         perror(a); exit(EXIT_FAILURE)

struct my_name_list{
    char* name;
    struct my_name_list *next;
};

void list_add_name(struct my_name_list **head, const char *new_name);
void list_destroy(struct my_name_list **head);
void list_print(struct my_name_list **n);
struct my_name_list* list_init(int max_name_length, const char *filepath);
bool list_populate(struct my_name_list **head);
void list_save(struct my_name_list **head);
bool list_contains_name(struct my_name_list **head, const char *name_to_find);
void list_remove_name(struct my_name_list **head, const char *name_to_remove, struct my_name_list *previous_node);

#endif /* linked_list_h */
