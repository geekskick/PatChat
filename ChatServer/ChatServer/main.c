#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

/*https://www.reddit.com/r/dailyprogrammer/comments/4knivr/20160523_challenge_268_easy_network_and_cards/ */

#define KILL(a)         perror(a); exit(EXIT_FAILURE)
#define NOT_CONNECTED   -1
#define NO_ID           0
#define NUM_MAX         5
#define MAX_USERS       10
#define MAX_NAME_LEN    50

struct my_thread_args{
    int my_thread_id_index;
    int client_fd_index;
};

struct my_buffer{
    char* buffer; //the string goes here
    int max_size; //the size of HEAP allocated
};

struct my_connection{
    char user_name[MAX_NAME_LEN];
    int socket_fd;
};

// ===== SHARED RESOURCES =====
pthread_mutex_t mutex;
int CLIENT_IDS[NUM_MAX];
pthread_t THREAD_IDS[NUM_MAX];
char *LOGIN_NAMES[MAX_USERS];
struct my_connection CURRENT_CONNECTIONS[NUM_MAX];

// ===== DECLARATIONS ========
void clear_client_id(int index);
void clear_thread_id(int index);
int get_client_fd_at_index(int index);
pthread_t get_thread_id_at_index(int index);
void handle_ping(int client_fd);
int next_free_name();
bool insert_new_user_at_index(int index, char *name);
void handle_login(struct my_buffer *buff, char *c, int my_fd, int my_thread_index, bool *flag);
void handle_new_user(struct my_buffer *buff, char *c, int my_fd, int my_thread_index, bool *logged_in_flag);
void handle_broadcast(struct my_buffer *buff, struct my_thread_args *args);
bool name_exists(char *name_string);
int get_socket_for_user(char* user_name);
void get_name_for_connection(int index, char *buff);
void handle_message_send(struct my_buffer *buff, int my_client_fd, int my_thread_id);
void handle_list(int my_fd);
void *connection_handler(void *ptr);
void set_thread_id_at_index(pthread_t handler_thread, int index);
void set_client_fd_at_index(int client_fd, int index);

// ===== FUNCTIONS ======
/* make the client at the specififed index not connected */
void clear_client_id(int index){
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&mutex);
    CLIENT_IDS[index] = NOT_CONNECTED;
    pthread_mutex_unlock(&mutex);
    
}

/* make the thread at the specififed index not active */
void clear_thread_id(int index){
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&mutex);
    THREAD_IDS[index] = NO_ID;
    pthread_mutex_unlock(&mutex);
}

/* get the client ID from the array of current clients */
int get_client_fd_at_index(int index){
    if(index >= NUM_MAX){ return NOT_CONNECTED; }
    int i = NOT_CONNECTED;
    pthread_mutex_lock(&mutex);
    i = CLIENT_IDS[index];
    pthread_mutex_unlock(&mutex);
    return i;
}

/* get the thread id from the array of current threads */
pthread_t get_thread_id_at_index(int index){
    if(index >= NUM_MAX){ return NO_ID; }
    
    pthread_t i = NO_ID;
    pthread_mutex_lock(&mutex);
    i = THREAD_IDS[index];
    pthread_mutex_unlock(&mutex);
    return i;
}

/* send pong to the client */
void handle_ping(int client_fd){
    write(client_fd, "PONG", strlen("PONG"));
}

/* Broadcast the message passed in to all currently connected clients */
void handle_broadcast(struct my_buffer *buff, struct my_thread_args *args){
    
    for(int i = 0; i < NUM_MAX; i++){
        int temp_fd = NOT_CONNECTED;
        
        //only send to the clients connected
        if((temp_fd = get_client_fd_at_index(i)) != NOT_CONNECTED){
            write(temp_fd, buff->buffer, strlen(buff->buffer));
        }
    }
}

/* if the name passed in it in the array then return true */
bool name_exists(char *name_string){
    bool rc = false;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_USERS; i++) {
        if(strcmp(name_string, LOGIN_NAMES[i]) == 0){
            rc = true;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return rc;
}

/* finds the next free index in the array of usernames */
int next_free_name(){
    int name = MAX_USERS;
    
    pthread_mutex_lock(&mutex);
    
    //Test the first character in each name, if it's not used then it's the next free name
    for(int i = 0; i < MAX_USERS; i++){
        if(LOGIN_NAMES[i][0] == 0){
            name = i;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return name;
}

/* inserts a new username in the array */
bool insert_new_user_at_index(int index, char *name){
    if(index >= MAX_USERS) { return false; }
    pthread_mutex_lock(&mutex);
    strcpy(LOGIN_NAMES[index], name);
    pthread_mutex_unlock(&mutex);
    return true;
}

/* log the user in if they are already registered */
void handle_login(struct my_buffer *buff, char *c, int my_fd, int my_thread_index, bool *flag){
    
    //The command is in the form LOGIN <name>, so find the whitespace and use the string after as the username
    strtok(buff->buffer, " ");
    char *space = strtok(NULL, " ");
    
    const char
    *LOGIN_SUCCESS = "LOGIN successful",
    *LOGIN_FAIL    = "LOGIN failed";
    
    //log in if they are registered
    if(name_exists(space)){
        write(my_fd, LOGIN_SUCCESS, strlen(LOGIN_SUCCESS));
        
        pthread_mutex_lock(&mutex);
        strcpy(CURRENT_CONNECTIONS[my_thread_index].user_name, space);
        CURRENT_CONNECTIONS[my_thread_index].socket_fd = my_fd;
        pthread_mutex_unlock(&mutex);
        
        *flag = true;
    }
    //doesnt exist
    else{
        write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL));
    }
    
}

/* Adds a new user to the username list and logs them in */
void handle_new_user(struct my_buffer *buff, char *c, int my_fd, int my_thread_index, bool *logged_in_flag){
    
    //The command is in the form NEWUSER <name>, so find the whitespace and use the string after as the username
    strtok(buff->buffer, " ");
    char *space = strtok(NULL, " ");
    
    const char
    *LOGIN_SUCCESS = "USER CREATION successful",
    *LOGIN_FAIL    = "USER CREATION failed";
    
    int u = 0;
    
    //If there isn't a space found then dont bother trying to add
    if(space){
        
        //only add if the username doesn't already exist and there is room in the array
        if(name_exists(space) || (u = next_free_name()) == MAX_USERS){
            write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL));
        }
        else{
            if(!insert_new_user_at_index(u, space)){
                write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL));
                return;
                
            }
            
            pthread_mutex_lock(&mutex);
            strcpy(CURRENT_CONNECTIONS[my_thread_index].user_name, space);
            CURRENT_CONNECTIONS[my_thread_index].socket_fd = my_fd;
            pthread_mutex_unlock(&mutex);
            
            write(my_fd, LOGIN_SUCCESS, strlen(LOGIN_SUCCESS));
            *logged_in_flag = true;
        }
    }
    else{
        write(my_fd, LOGIN_FAIL, strlen(LOGIN_FAIL));
    }
}

//Gets the socket number of the user name passed in
int get_socket_for_user(char* user_name){
    int sock = NOT_CONNECTED;
    pthread_mutex_lock(&mutex);
    
    for(int i = 0; i < NUM_MAX; i++){
        if(strcmp(user_name, CURRENT_CONNECTIONS[i].user_name) == 0){
            sock = CURRENT_CONNECTIONS[i].socket_fd;
        }
    }
    
    pthread_mutex_unlock(&mutex);
    return sock;
}

//Get the user name at the index of the current connections
void get_name_for_connection(int index, char *buff){
    pthread_mutex_lock(&mutex);
    strcpy(buff, CURRENT_CONNECTIONS[index].user_name);
    pthread_mutex_unlock(&mutex);
}

/* relays a message from the client to another conected client */
void handle_message_send(struct my_buffer *buff, int my_client_fd, int my_thread_id){
    
    strtok(buff->buffer, "$");
    char *dest = strtok(NULL, "$");
    
    //if the user didn't enter the command in the form SEND $<user> <msg> then reminde them to use a $
    if(!dest){ write(my_client_fd, "$$$$", strlen("$$$$")); return; }
    
    char name_buff[MAX_NAME_LEN] = { 0, };
    
    //Extract the destination user name
    strcpy(name_buff, dest);
    strtok(name_buff, " ");
    char* msg = strtok(NULL, " ");
    
    //Get the origin user's name
    char reply[1000] = { 0, }, relay_msg[1000] = { 0, };
    char origin_user[MAX_NAME_LEN] = { 0, };
    get_name_for_connection(my_thread_id, origin_user);
    
    //construct messages
    snprintf(reply, 1000, "SENT[%s]: %s\n", name_buff, msg);
    snprintf(relay_msg, 1000, "RECD[%s]: %s\n", origin_user, msg);
    
    //Send the message if the desination user is connected
    int dest_fd = get_socket_for_user(name_buff);
    if(dest_fd == NOT_CONNECTED){
        write(my_client_fd, "Fail", strlen("Fail"));
    }
    else{
        printf("%s\n", reply);
        write(my_client_fd, reply, strlen(reply)); //echo to sender
        write(dest_fd, relay_msg, strlen(relay_msg));
    }
}

/* Create a list of currently connected usernames and send them to the client */
void handle_list(int my_fd){
    
    char b[MAX_NAME_LEN * (NUM_MAX + 1)] = { 0, };
    pthread_mutex_lock(&mutex);
    
    for(int i = 0; i < NUM_MAX; i++){
        if(CURRENT_CONNECTIONS[i].socket_fd != NOT_CONNECTED){
            strcat(b, CURRENT_CONNECTIONS[i].user_name);
            strcat(b, "\n");
        }
    }
    
    pthread_mutex_unlock(&mutex);
    
    write(my_fd, b, strlen(b));
    
}

/* once a thread is created is comes here */
void *connection_handler(void *ptr){
    printf("Thread created\n");
    
    //cast the incoming struct to access it's contents
    struct my_thread_args *args = (struct my_thread_args*)ptr;
    
    //create space for the reieved data
    struct my_buffer buff;
    buff.buffer = calloc(sizeof(char), 3000);
    buff.max_size = 3000;
    
    //printf("thread_ids[%d] = %d\n", args->my_id, args->all_ids[args->my_id]);
    
    long read_size = 0;
    int my_client_fd = get_client_fd_at_index(args->client_fd_index);
    char *c = NULL;
    bool logged_in = false;
    
    //keep reading into the buffer until nothing is read anymore OR until the PING of BROADCAST commands are recieved
    while((read_size = recv(my_client_fd, buff.buffer, buff.max_size, 0)) > 0){
        
        if(strstr(buff.buffer, "BeatReply")){
            printf("ACK recd\n");
            
        }
        else if(logged_in){
            if(strstr(buff.buffer, "PING")){
                printf("ping recvd\n");
                handle_ping(my_client_fd);
            }
            else if(strstr(buff.buffer, "BROADCAST")){
                printf("bcast request recvd\n");
                handle_broadcast(&buff, args);
            }
            else if(strstr(buff.buffer, "LOGOUT")){
                break;
            }
            else if(strstr(buff.buffer, "SEND")){
                printf("message relay request recd\n");
                handle_message_send(&buff, my_client_fd, args->my_thread_id_index);
            }
            else if(strstr(buff.buffer, "LIST")){
                printf("List recd\n");
                handle_list(my_client_fd);
                
            }
            else{
                write(my_client_fd, "Computer Says No", strlen("Computer Says No"));
            }
        }
        else if ((c = strstr(buff.buffer, "LOGIN"))){
            handle_login(&buff, c, my_client_fd, args->my_thread_id_index, &logged_in);
        }
        else if((c = strstr(buff.buffer, "NEWUSER"))){
            handle_new_user(&buff, c, my_client_fd, args->my_thread_id_index, &logged_in);
        }
        else{
            write(my_client_fd, "You must log in or create a new user", strlen("You must log in or create a new user"));
        }
        
        
    }
    
    if(read_size == 0){
        printf("client disconnected\n");
    }
    else{
        close(my_client_fd);
        printf("disconnecting client\n");
    }
    
    memset(CURRENT_CONNECTIONS[args->my_thread_id_index].user_name, 0, MAX_NAME_LEN);
    CURRENT_CONNECTIONS[args->my_thread_id_index].socket_fd = 0;
    
    //reset the thread id and client fd
    clear_client_id(args->client_fd_index);
    clear_thread_id(args->my_thread_id_index);
    
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
    
    pthread_mutex_lock(&mutex);
    THREAD_IDS[index] = handler_thread;
    pthread_mutex_unlock(&mutex);
}

/* put a client id into the array */
void set_client_fd_at_index(int client_fd, int index) {
    if(index >= NUM_MAX) { return; }
    
    pthread_mutex_lock(&mutex);
    CLIENT_IDS[index] = client_fd;
    pthread_mutex_unlock(&mutex);
}

/* sends "Beat" to the clients every 5 seconds */
void *heartbeat_thread(){
    
    struct timespec time_to_wait, time_remaining;
    time_to_wait.tv_sec = 5;
    time_to_wait.tv_nsec = 0;
    
    while(1){
        for(int i = 0; i < NUM_MAX; i++){
            int f;
            if((f = get_client_fd_at_index(i)) != NOT_CONNECTED){
                printf("Heartbeat to %d\n", f);
                write(f, "Beat", strlen("Beat"));
            }
        }
        
        nanosleep(&time_to_wait, &time_remaining);
    }
}

int main(void){
    
    printf("Starting program\n");
    
    const int PORT_NUM = 51718;
    int       server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t heartbeat;
    
    //init shared resources
    for(int i = 0; i < NUM_MAX; i++){
        CLIENT_IDS[i] = NOT_CONNECTED;
        THREAD_IDS[i] = NO_ID;
    }
    
    for(int i = 0; i < MAX_USERS; i++){
        LOGIN_NAMES[i] = calloc(MAX_NAME_LEN, sizeof(char));
        if(!LOGIN_NAMES[i]) { KILL("Calloc Error\n"); }
    }
    
    
    //TCP IP Server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) { KILL("Error creating socket"); }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUM);
    
    if( bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { KILL("Binding Error"); }
    
    pthread_mutex_init(&mutex, NULL);
    
    listen(server_fd, NUM_MAX);
    printf("Waiting for connections on port %d\n", PORT_NUM);
    
    if(pthread_create(&heartbeat, NULL, heartbeat_thread, NULL)< 0) { KILL("Heartbeat creation"); }
    
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
        args->my_thread_id_index = current_id;
        args->client_fd_index = current_client;
        
        //return -1 if error
        if(pthread_create(&handler_thread, NULL, connection_handler, (void*)args) < 0){ KILL("Thread creation"); }
        
    }
    
    // ----- shut down safely ----
    for(int i = 0; i < MAX_USERS; i++){
        free(LOGIN_NAMES[i]);
    }
    
    //wait for all threads to finish
    for(int i = 0; i < NUM_MAX; i++){
        pthread_join(get_thread_id_at_index(i), NULL);
    }
    
    pthread_kill(heartbeat, 0);
    pthread_join(heartbeat, NULL);
    
    pthread_mutex_destroy(&mutex);
    exit(EXIT_SUCCESS);
}