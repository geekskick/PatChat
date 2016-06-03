//
//  linked_list.c
//  ChatServer
//
//  Created by Patrick Mintram on 30/05/2016.
//  Copyright © 2016 Patrick Mintram. All rights reserved.
//

#include "linked_list.h"

//private functions
struct my_name_list* create_node(const char *name);
struct my_name_list* find_last_node(struct my_name_list **head);
char *file_path;
int MAX_NAME_LEN = 0;

bool list_contains_name(struct my_name_list **head, const char *name_to_find){
    
    struct my_name_list* current_node = *head;
    
    if(name_to_find){
        if(strcmp(name_to_find, current_node->name) == 0){
            return true;
        }
        else if(current_node->next){
            return list_contains_name(&(current_node->next), name_to_find);
        }
    }
    
    return false;
    
}

//writes the link list contents to a file in the
void list_save(struct my_name_list **head){
    
    FILE* fp = fopen(file_path, "w");
    if(!fp){
        perror("Error opening file for writing\n");
        return;
    }
    
    struct my_name_list *current_node = *head;
    
    while(current_node){
        fwrite(current_node->name, 1, strlen(current_node->name), fp);
        fprintf(fp, "\n");
        current_node = current_node->next;
    }
    
    fclose(fp);
}

//reads from the file
bool list_populate(struct my_name_list **head){
    
    FILE *fp = fopen(file_path, "r+");
    int letter = 0;
    char *temp_name = calloc(MAX_NAME_LEN, sizeof(char));
    char c;
    
    if(!temp_name){ KILL("Error callocing name buffer\n"); }
    
    if(!fp){
        perror("File not opened\n");
        return false;
    }
    
    //until the file is done read in characters
    while((c = fgetc(fp)) != EOF){
        
        //if it's a newline then add the temp name buffer to the linked list
        if(c == '\n'){
            letter = 0;
            list_add_name(head, temp_name);
            memset(temp_name, 0, MAX_NAME_LEN );
        }
        //add to the temp name buffer while there is room
        else if(letter <= MAX_NAME_LEN){
            temp_name[letter++] = c;
        }
        
    }
    
    fclose(fp);
    
    free(temp_name);
    return true;
    
}

//initialise the max length of the nodes and pass back a null pointer
struct my_name_list* list_init(int max_name_length, const char *filepath){
    MAX_NAME_LEN = max_name_length;
    
    file_path = calloc(strlen(filepath), sizeof(char));
    if(!file_path){ KILL("Error callocing filepath memory\n"); }
    
    strcpy(file_path, filepath);
    
    struct my_name_list* first = NULL;
    return first;
}

/* creats HEAP memory for a nose and returns it's address */
struct my_name_list* create_node(const char *name){
    struct my_name_list *new_node = calloc(1, sizeof(struct my_name_list));
    if(!new_node) { KILL("Error in node calloc"); }
    
    new_node->name = calloc(MAX_NAME_LEN, sizeof(char));
    if(!new_node->name){ KILL("Error calloc-ing name in node"); }
    
    strcpy(new_node->name, name);
    new_node->next = NULL;
    
    return new_node;
}

/* check the value in node->next, if it's null then returns the node as thas the last item in the list */
struct my_name_list* find_last_node(struct my_name_list **head){
    
    if(!*head) {
        return NULL;
    }
    
    struct my_name_list *current_node = *head;
    
    while(current_node->next){
        current_node = current_node->next;
    }
    
    return current_node;
    
}

/* Adds a name to the end of the list */
void list_add_name(struct my_name_list **head, const char *new_name){
    
    //Create a new node containing the new username
    struct my_name_list *current_node;
    
    //if it's the first node in the list
    if(!*head){
        *head = create_node(new_name);
    }
    //its not the first node
    else{
        current_node = find_last_node(head);
        current_node->next = create_node(new_name);
    }
    
}

/* free's memory used by the list */
void list_destroy(struct my_name_list **head){
    struct my_name_list *node = *head;
    
    if(node->next){
        list_destroy(&node->next);
    }
    
    free(node->name);
    node->name = NULL;
    
    free(node);
    node = NULL;
    
    if(file_path){
        free(file_path);
        file_path = NULL;
    }
}

/* prints the list contents */
void list_print(struct my_name_list **n){
    
    struct my_name_list *node = *n;
    
    if(node){
        printf("%s\n", node->name);
        list_print(&node->next);
    }
    
}