#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "linked_list.h"

/*https://www.reddit.com/r/dailyprogrammer/comments/4knivr/20160523_challenge_268_easy_network_and_cards/ */

#define KILL(a)         perror(a); exit(EXIT_FAILURE)
#define NOT_CONNECTED   -1
#define NO_ID           0
#define NUM_MAX         5
#define MAX_USERS       10
#define MAX_NAME_LEN    50
#define MAX_BUFF_SIZE   3000

struct my_thread_args{
    int current_connection_index;
};

struct my_buffer{
    char* buffer; //the string goes here
    int max_size; //the size of HEAP allocated
};

struct my_options{
    bool echo;      //if the client wants it's messages echo'd this is true
    bool silent;    //if this is true it will override all messages to sender
};

struct my_connection{
    char user_name[MAX_NAME_LEN];
    int socket_fd;
    pthread_t my_thread_id;
    struct my_options opt;
};


// ===== SHARED RESOURCES =====
pthread_mutex_t SHARED_MUTEX;
volatile struct my_connection SHARED_CURRENT_CONNECTIONS[NUM_MAX]; //memory alreasy allocated on stack  so no need to calloc of malloc
volatile struct my_name_list *SHARED_NAMES_LIST = NULL;
char *SHARED_HELP_FILE;

// ===== DECLARATIONS ========
void clear_connection_at_index(int index);
int get_client_fd_at_index(int index);
pthread_t get_thread_id_at_index(int index);
void handle_ping(int client_fd);
void handle_login(struct my_buffer *buff, char *c, int my_fd, int my_conn_index, bool *flag);
void handle_new_user(struct my_buffer *buff, char *c, int my_fd, int my_conn_index, bool *logged_in_flag);
void handle_broadcast(struct my_buffer *buff, struct my_thread_args *args);
int get_conn_index_for_user(char* user_name);
void get_name_for_connection_at_index(int index, char *buff);
void handle_message_send(struct my_buffer *buff, int my_conn_index);
void handle_list(int my_conn_index);
void *connection_handler(void *ptr);
void set_thread_id_at_index(pthread_t handler_thread, int index);
void set_client_fd_at_index(int client_fd, int index);
void send_msg(int conn_index, const char *msg);
bool connection_is_silent(int current_connection_id);
void toggle_silent(int current_connection_id);
void set_connection_opt_silent(int current_connection_id, bool state);
bool get_connection_opt_echo(int current_connection_id);
void set_connection_opt_echo(int current_connection_id, bool state);
void toggle_echo(int current_connection_id);

// ===== FUNCTIONS ======
/* make the client at the specififed index not connected */
void clear_connection_at_index(int index){
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&SHARED_MUTEX);
    
    SHARED_CURRENT_CONNECTIONS[index].socket_fd = NOT_CONNECTED;
    memset(SHARED_CURRENT_CONNECTIONS[index].user_name, 0, MAX_NAME_LEN);
    SHARED_CURRENT_CONNECTIONS[index].my_thread_id = NO_ID;
    
    pthread_mutex_unlock(&SHARED_MUTEX);
}


/* get the client ID from the array of current clients */
int get_client_fd_at_index(int index){
    if(index >= NUM_MAX){ return NOT_CONNECTED; }
    int i = NOT_CONNECTED;
    
    pthread_mutex_lock(&SHARED_MUTEX);
    i = SHARED_CURRENT_CONNECTIONS[index].socket_fd;
    pthread_mutex_unlock(&SHARED_MUTEX);
    return i;
}

/* get the thread id from the array of current threads */
pthread_t get_thread_id_at_index(int index){
    if(index >= NUM_MAX){ return NO_ID; }
    
    pthread_t i = NO_ID;
    pthread_mutex_lock(&SHARED_MUTEX);
    i = SHARED_CURRENT_CONNECTIONS[index].my_thread_id;
    pthread_mutex_unlock(&SHARED_MUTEX);
    return i;
}

/* send pong to the client */
void handle_ping(int client_index){
    send_msg(client_index, "PONG");
}

/* Broadcast the message passed in to all currently connected clients */
void handle_broadcast(struct my_buffer *buff, struct my_thread_args *args){
    
    for(int i = 0; i < NUM_MAX; i++){
        send_msg(i, buff->buffer);
    }
}


/* log the user in if they are already registered */
void handle_login(struct my_buffer *buff, char *c, int my_fd, int my_conn_index, bool *flag){
    
    //The command is in the form LOGIN <name>, so find the whitespace and use the string after as the username
    strtok(buff->buffer, " ");
    char *sent_name = strtok(NULL, " ");
    
    const char
    *LOGIN_SUCCESS = "LOGIN successful",
    *LOGIN_FAIL    = "LOGIN failed - username not found";
    
    //if the user sends LOGIN and no name the sent_name pointer will be NULL, causing an error in the linked list library, so check before it's sent for checking
    if(sent_name){
        pthread_mutex_lock(&SHARED_MUTEX);
        list_print(&SHARED_NAMES_LIST);
        
        if(list_contains_name(&SHARED_NAMES_LIST, sent_name)){
            write(my_fd, LOGIN_SUCCESS, strlen(LOGIN_SUCCESS)); //mutex already locked so no need to lock it
            
            //as the mutex is already locked this is handled here rather than an accessor function
            strcpy(SHARED_CURRENT_CONNECTIONS[my_conn_index].user_name, sent_name);
            SHARED_CURRENT_CONNECTIONS[my_conn_index].socket_fd = my_fd;
            *flag = true;
        }
        else{
            write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL)); //mutex already locked so no need to lock it
        }
        pthread_mutex_unlock(&SHARED_MUTEX);
    }
    //doesnt exist
    else{
        send_msg(my_conn_index, LOGIN_FAIL);
    }

    
}

/* Adds a new user to the username list and logs them in */
void handle_new_user(struct my_buffer *buff, char *c, int my_fd, int my_conn_index, bool *logged_in_flag){
    
    //The command is in the form NEWUSER <name>, so find the whitespace and use the string after as the username
    strtok(buff->buffer, " ");
    char *sent_name = strtok(NULL, " ");
    
    const char
    *LOGIN_SUCCESS = "USER CREATION successful",
    *LOGIN_FAIL    = "USER CREATION failed - username already exists, try logging in";
    
    //If there isn't a space found then dont bother trying to add
    if(sent_name){
        
        //only add if the username doesn't already exist
        pthread_mutex_lock(&SHARED_MUTEX);
        if(list_contains_name(&SHARED_NAMES_LIST, sent_name)){
            write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL)); //mutex already locked so no need to lock it
        }
        else{
            
            list_add_name(&SHARED_NAMES_LIST, sent_name);
            list_save(&SHARED_NAMES_LIST);
            
            //as the mutex is already locked this is handled here rather than an accessor function
            strcpy(SHARED_CURRENT_CONNECTIONS[my_conn_index].user_name, sent_name);
            SHARED_CURRENT_CONNECTIONS[my_conn_index].socket_fd = my_fd;
            
            write(my_fd, LOGIN_SUCCESS, strlen(LOGIN_SUCCESS)); //mutex already locked so no need to lock it
            *logged_in_flag = true;
        }
        pthread_mutex_unlock(&SHARED_MUTEX);
    }
    else{
        send_msg(my_conn_index, LOGIN_FAIL);
    }
}

//Gets the socket number of the user name passed in
int get_conn_index_for_user(char* user_name){
    int index = NOT_CONNECTED;
    pthread_mutex_lock(&SHARED_MUTEX);
    
    for(int i = 0; i < NUM_MAX; i++){
        if(strcmp(user_name, SHARED_CURRENT_CONNECTIONS[i].user_name) == 0){
            index = i;
        }
    }
    
    pthread_mutex_unlock(&SHARED_MUTEX);
    return index;
}

//Get the user name at the index of the current connections
void get_name_for_connection_at_index(int index, char *buff){
    pthread_mutex_lock(&SHARED_MUTEX);
    strcpy(buff, SHARED_CURRENT_CONNECTIONS[index].user_name);
    pthread_mutex_unlock(&SHARED_MUTEX);
}

/* relays a message from the client to another conected client */
void handle_message_send(struct my_buffer *buff, int my_conn_index){
    
    int my_client_fd= get_client_fd_at_index(my_conn_index);
    
    strtok(buff->buffer, "$");
    char *dest = strtok(NULL, "$");
    
    const char *USAGE_MSG = "Usage: SEND $<name> <message to send>";
    
    //if the user didn't enter the command in the form SEND $<user> <msg> then reminde them to use a $
    if(!dest){ send_msg(my_client_fd, USAGE_MSG); return; }
    
    char name_buff[MAX_NAME_LEN] = { 0, };
    
    //Extract the destination user name
    strcpy(name_buff, dest);
    strtok(name_buff, " ");
    char* msg = strtok(NULL, " ");
    
    //Get the origin user's name
    char reply[MAX_BUFF_SIZE] = { 0, }, relay_msg[MAX_BUFF_SIZE] = { 0, }, origin_user[MAX_NAME_LEN] = { 0, };
    
    get_name_for_connection_at_index(my_conn_index, origin_user);

    sprintf(reply,"SENT[%s]: ", name_buff);         //initialise the echo message and message to relay
    sprintf(relay_msg, "RECD[%s]: ", origin_user);
    
    // The white space in the message means it can't be a simple strcpy over once, so
    // the array must be iterated over adding each word at a time - until there is no more whitespace found
    
    while(msg != NULL){
        strcat(relay_msg, " "); //put a gap before the next word added to preserve whitespace
        strcat(reply, " ");
        strcat(relay_msg, msg); //add next word
        strcat(reply, msg);
        msg = strtok(NULL, " "); //advance to next whitespace location
    }

    
    //Send the message if the desination user is connected
    int dest_conn_index = get_conn_index_for_user(name_buff);
    if(dest_conn_index == NOT_CONNECTED){
        send_msg(my_conn_index, "Fail");
    }
    else{
        printf("%s\n", reply);
        
        if(get_connection_opt_echo(my_conn_index)){
            send_msg(my_conn_index, reply);
        }
        
        
        send_msg(dest_conn_index, relay_msg);
    }
}

/* Create a list of currently connected usernames and send them to the client */
void handle_list(int my_conn_index){
    printf("list\n");
    const char *HEADER_MSG = "Currently connected are:\n";
    
    char b[MAX_NAME_LEN * (NUM_MAX + 1) + 30] = { 0, }; //30 characters should be enough to keep the header line in, no need to malloc a small amount only
    strcat(b, HEADER_MSG);
    
    pthread_mutex_lock(&SHARED_MUTEX);
    
    //Add new line to the list for each user connected
    for(int i = 0; i < NUM_MAX; i++){
        if(SHARED_CURRENT_CONNECTIONS[i].socket_fd != NOT_CONNECTED){
            strcat(b, SHARED_CURRENT_CONNECTIONS[i].user_name);
            strcat(b, "\n");
        }
    }
    
    pthread_mutex_unlock(&SHARED_MUTEX);
    
    //remover final new line to keep it pretty for the client
    char *last_known_nl = strrchr(b, '\n');
    if(last_known_nl){
        *last_known_nl = '\0';
    }
    
    send_msg(my_conn_index, b);
    
    printf("list end\n");
}



void toggle_silent(int current_connection_id){
    //printf("My conn_ind:\t %d\n", current_connection_id);
    assert(current_connection_id < NUM_MAX);
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.silent = !SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.silent;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

bool connection_is_silent(int current_connection_id){
    //printf("My conn_ind:\t %d\n", current_connection_id);
    assert(current_connection_id < NUM_MAX);
    bool rc;
    pthread_mutex_lock(&SHARED_MUTEX);
    rc = SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.silent;
    pthread_mutex_unlock(&SHARED_MUTEX);
    return rc;
}

void set_connection_opt_silent(int current_connection_id, bool state){
    assert(current_connection_id < NUM_MAX);
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.silent = state;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

void toggle_echo(int current_connection_id){
    assert(current_connection_id < NUM_MAX);
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.echo = !SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.echo;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

bool get_connection_opt_echo(int current_connection_id){
    assert(current_connection_id < NUM_MAX);
    bool rc;
    pthread_mutex_lock(&SHARED_MUTEX);
    rc = SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.echo;
    pthread_mutex_unlock(&SHARED_MUTEX);
    return rc;
}

void set_connection_opt_echo(int current_connection_id, bool state){
    assert(current_connection_id < NUM_MAX);
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[current_connection_id].opt.echo = state;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

void send_msg(int conn_index, const char *msg){
    if(!connection_is_silent(conn_index)){
        int fd;
        if((fd = get_client_fd_at_index(conn_index)) != NOT_CONNECTED){
            
            printf("Sending %s to fd:\t%d\n", msg, fd);
            pthread_mutex_lock(&SHARED_MUTEX);
            write(fd, msg, strlen(msg));
            pthread_mutex_unlock(&SHARED_MUTEX);
        }
    }
}

void help_file_init(){
    FILE *fp = fopen("helpfile.txt", "r");
    if(!fp){ return; }
    pthread_mutex_lock(&SHARED_MUTEX);
    
    struct stat s;
    stat("helpfile.txt", &s);
    off_t len = s.st_size;
    
    printf("Th file is %lld long\n", len);
    
    SHARED_HELP_FILE = calloc(len, sizeof(char));
    
    if(!SHARED_HELP_FILE){ KILL("ERROR allocation of help file"); }
    
    fread(SHARED_HELP_FILE, len, sizeof(char), fp);
    
    pthread_mutex_unlock(&SHARED_MUTEX);
    fclose(fp);
}


/* if the user enters OPTIONS */
void handle_options(struct my_buffer *buff, int my_conn_index){
    
    const char TOKEN = '$';
    const char* SILENT_ON_MSG = "Silent: ON";
    const char* SILENT_OFF_MSG = "Silent: OFF";
    const char* ECHO_ON_MSG = "Echo: ON";
    const char* ECHO_OFF_MSG = "Echo: OFF";
    
    strtok(buff->buffer, &TOKEN);
    char *arg = strtok(NULL, &TOKEN);
    
    while(arg){
        if(strstr(arg, "SILENT")){
            
            //toggle silent
            if(!strstr(arg,"?")){
                toggle_silent(my_conn_index);
            }
            
            //report the current silent state
            bool new_silent_state = connection_is_silent(my_conn_index);
            send_msg(my_conn_index, new_silent_state? SILENT_ON_MSG : SILENT_OFF_MSG);

        }
        
        else if (strstr(arg, "ECHO")){
            
            //toggle echo
            if(!strstr(arg, "?")){
                toggle_echo(my_conn_index);
            }
            
            //report the current echo state
            bool new_tog_State = get_connection_opt_echo(my_conn_index);
            send_msg(my_conn_index, new_tog_State? ECHO_ON_MSG: ECHO_OFF_MSG);
            
        }
        
        else if(strstr(arg, "ME?")){
            char my_name[MAX_NAME_LEN] = { 0, };
            get_name_for_connection_at_index(my_conn_index, my_name);
            send_msg(my_conn_index, my_name);
        }
        
        else if(strstr(arg, "HELP")){
            send_msg(my_conn_index, SHARED_HELP_FILE);
        }
        
        else if(strstr(arg, "NEWNAME")){
            bool changed = false;
            char *name;
            const char SPACE = ' ';
            strtok(arg, &SPACE);
            name = strtok(NULL, &SPACE);
            
            while(name){
                if(!strstr(name, &TOKEN)){
                    
                    //no need to check the name is in the list becuase this was donw before to log the user in
                    pthread_mutex_lock(&SHARED_MUTEX);
                    list_remove_name(&SHARED_NAMES_LIST, SHARED_CURRENT_CONNECTIONS[my_conn_index].user_name, NULL);
                    list_add_name(&SHARED_NAMES_LIST, name);
                    list_print(&SHARED_NAMES_LIST);
                    list_save(&SHARED_NAMES_LIST);
                    pthread_mutex_unlock(&SHARED_MUTEX);
                    
                    send_msg(my_conn_index, "Changed yo' name");
                    changed = true;
                    break;
                }
                else{
                    name = strtok(NULL, &SPACE);
                }
            }
            if(!changed){ send_msg(my_conn_index, "Name not changed"); }
        }
        
        else{
            //head from help file and send contents here
            send_msg(my_conn_index, "Not a valid option help file follows!");
        }

        printf("option: %s\n", arg);
        
        arg = strtok(NULL, &TOKEN);
    }
    
    
}

/* once a thread is created is comes here */
void *connection_handler(void *ptr){
    printf("Thread created\n");
    
    //cast the incoming struct to access it's contents
    struct my_thread_args *args = (struct my_thread_args*)ptr;
    
    //create space for the received data
    struct my_buffer buff;
    buff.buffer = calloc(sizeof(char), MAX_BUFF_SIZE);
    buff.max_size = MAX_BUFF_SIZE;
    
    //printf("thread_ids[%d] = %d\n", args->my_id, args->all_ids[args->my_id]);
    
    long read_size = 0;
    int my_client_fd = get_client_fd_at_index(args->current_connection_index);
    char *c = NULL;
    bool logged_in = false;
    
    //keep reading into the buffer until nothing is read anymore OR until the PING of BROADCAST commands are recieved
    while((read_size = recv(my_client_fd, buff.buffer, buff.max_size, 0)) > 0){
        
        printf("recd: %s\n", buff.buffer);
        
        //The heartbeat ACK is not that important so in this case put it first
        if(strstr(buff.buffer, "ACK")){
            /* put some handling here */
            printf("ACK recd\n");
        }
        
        //A kenny loggin'd in user has access to the commands of the chat
        else if(logged_in){
            if(strstr(buff.buffer, "OPT")){
                printf("options rcd\n");
                handle_options(&buff, args->current_connection_index);
            }
            else if(strstr(buff.buffer, "PING")){
                printf("ping recvd\n");
                handle_ping(args->current_connection_index);
            }
            else if(strstr(buff.buffer, "BROADCAST")){
                printf("bcast request recvd\n");
                handle_broadcast(&buff, args);
            }
            else if(strstr(buff.buffer, "LOGOUT")){
                printf("logout recd\n");
                break;
            }
            else if(strstr(buff.buffer, "SEND")){
                printf("message relay request recd\n");
                handle_message_send(&buff, args->current_connection_index);
            }
            else if(strstr(buff.buffer, "LIST")){
                printf("List recd\n");
                handle_list(args->current_connection_index);
                
            }
            else{
                send_msg(args->current_connection_index, "Type \"OPT $HELP\" for commands");
            }
        }
        else if ((c = strstr(buff.buffer, "LOGIN"))){
            handle_login(&buff, c, my_client_fd, args->current_connection_index, &logged_in);

        }
        else if((c = strstr(buff.buffer, "NEWUSER"))){
            handle_new_user(&buff, c, my_client_fd, args->current_connection_index, &logged_in);
        }
        else{
            send_msg(args->current_connection_index, "You must log in or create a new user");
        }
        
        /*
         clear buffer after the read in string is handled to prevent persistent messages eg
         if a user sends 
         
         "NEWUSER helloyou" then 
         "LOGIN helloyou", 
         
         the second message is shorter so the buffer will actually
         contain "LOGIN helloyouou"!
        */
        memset(buff.buffer, 0, buff.max_size);
    }
    
    if(read_size == 0){
        printf("client disconnected\n");
    }
    else{
        close(my_client_fd);
        printf("disconnecting client\n");
    }
    
    
    //reset the thread id and current connection
    clear_connection_at_index(args->current_connection_index);
    
    //free memory
    free(args);
    free(buff.buffer);
    
    args = NULL;
    buff.buffer = NULL;
    printf("Thread ending\n");
    pthread_exit(NULL);
}

/* put a thread id into the array */
void set_thread_id_at_index(pthread_t handler_thread, int index) {
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[index].my_thread_id = handler_thread;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

/* put a client id into the array */
void set_client_fd_at_index(int client_fd, int index) {
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&SHARED_MUTEX);
    SHARED_CURRENT_CONNECTIONS[index].socket_fd = client_fd;
    pthread_mutex_unlock(&SHARED_MUTEX);
}

/* sends "Beat" to the clients every 5 seconds */
void *heartbeat_thread(){
    
    struct timespec time_to_wait, time_remaining;
    time_to_wait.tv_sec = 5;
    time_to_wait.tv_nsec = 0;
    
    while(1){
        for(int i = 0; i < NUM_MAX; i++){
            //printf("Heartbeat to fd:%d\n", get_client_fd_at_index(i));
            send_msg(i, "BEAT");
        }
        
        nanosleep(&time_to_wait, &time_remaining);
    }
}


void test_bench(){
    struct my_name_list *n = list_init(MAX_NAME_LEN, "usernames.txt");
    
    list_add_name(&n, "PoopBum");
    list_add_name(&n, "Bumper");
    list_populate(&n);
    bool m = list_contains_name(&n, "Poohole");
    bool mm = list_contains_name(&n, "Anus");
    list_add_name(&n, "Anus");
    list_print(&n);
    
    list_save(&n);
    
    list_destroy(&n);

    exit(EXIT_SUCCESS);
}

void *get_stop_cmd(){
    //something to make the program quite safely in here
    while(1){}
}

void init_resources(){
    //init shared resources
    for(int i = 0; i < NUM_MAX; i++){
        SHARED_CURRENT_CONNECTIONS[i].socket_fd    = NOT_CONNECTED;
        SHARED_CURRENT_CONNECTIONS[i].my_thread_id = NO_ID;
        SHARED_CURRENT_CONNECTIONS[i].opt.echo = false;
        SHARED_CURRENT_CONNECTIONS[i].opt.silent = false;
    }
    
    SHARED_NAMES_LIST = list_init(MAX_NAME_LEN, "usernames.txt");
    list_populate(&SHARED_NAMES_LIST);
    help_file_init();
    
}
int main(void){
    
    printf("Starting program\n");
    //test_bench();
    
    const int PORT_NUM = 51718;
    int       server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t heartbeat;
    pthread_t stop_thread;
    
    init_resources();
    
    //TCP IP Server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) { KILL("Error creating socket"); }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUM);
    
    if( bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { KILL("Binding Error"); }
    
    pthread_mutex_init(&SHARED_MUTEX, NULL);
    
    listen(server_fd, NUM_MAX);
    printf("Waiting for connections on port %d\n", PORT_NUM);
    
    if(pthread_create(&heartbeat, NULL, heartbeat_thread, NULL) < 0) { KILL("Heartbeat creation"); }
    if(pthread_create(&stop_thread, NULL, get_stop_cmd, NULL) < 0){ KILL("Stop creation"); }
    
    int clilen = sizeof(client_addr);
    
    while((client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &clilen))){
        pthread_t handler_thread;
        int current_id = 0;
        int current_client = 0;
        
        //find the next available handler id locations and clienf fd location in the arrays
        while(get_thread_id_at_index(current_id) != NO_ID && current_id < NUM_MAX){ current_id++; }
        while(get_client_fd_at_index(current_client) > NOT_CONNECTED && current_client < NUM_MAX) { current_client++; }
        if(current_client > NUM_MAX || current_id > NUM_MAX){
            KILL("Error storing thread ids and client fds");
        }
        
        set_thread_id_at_index(handler_thread, current_id);
        set_client_fd_at_index(client_fd, current_client);
        
        //construct thread args, free'd in the thread
        struct my_thread_args *args = calloc(1, sizeof(args));
        args->current_connection_index = current_client;
        
        //return -1 if error
        if(pthread_create(&handler_thread, NULL, connection_handler, (void*)args) < 0){ KILL("Thread creation"); }
        
    }
    
    //wait for all threads to finish
    for(int i = 0; i < NUM_MAX; i++){
        pthread_join(get_thread_id_at_index(i), NULL);
    }
    
    pthread_cancel(heartbeat);
    pthread_cancel(stop_thread);
    
    pthread_join(heartbeat, NULL);
    pthread_join(stop_thread, NULL);
    
    pthread_mutex_destroy(&SHARED_MUTEX);
    
    list_destroy(&SHARED_NAMES_LIST);
    free(SHARED_HELP_FILE);
    exit(EXIT_SUCCESS);
}