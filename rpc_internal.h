//
// Created by alanz on 5/21/23.
//

#ifndef COMP30023_2023_PROJECT_2_RPC_INTERNAL_H
#define COMP30023_2023_PROJECT_2_RPC_INTERNAL_H

#include <stdint.h>
#include <netinet/in.h>
#include "rpc.h"

#define NONBLOCKING
#define RPC_ERROR 0
#define RPC_SUCCESS 1
#define RPC_FIND 2
#define RPC_CALL 3


struct connection_args {
    rpc_server *srv;
    int client_sock;
};

struct rpc_client {
    int is_connected;
    struct sockaddr_in6 server_addr;
};

struct rpc_handle {
    char *function_name;
};

struct rpc_server {
    int server_sock;
    struct function_reg *registered_functions;
    int is_running;
};

/* Using a linked list to store all the registered function for the server */
typedef struct function_reg {
    char *function_name;
    rpc_data* (*handler)(rpc_data*);
    struct function_reg *next;
} function_reg;


/* Helper function to convert 8-byte integer to network byte order */
uint64_t htonll(uint64_t value);

/* Helper function to convert network byte order to 8-byte integer */
uint64_t ntohll(uint64_t value);

/* Helper function to send message using designed protocol */
int rpc_send_message(int sock, int operation, char *name, rpc_data *data);

/* Helper function to receive message using designed protocol */
int read_message(int client_sock, int *operation, size_t *name_len, size_t *data_len, char **function_name);

/* Helper function to create client socket and connect with server */
int create_and_connect_socket(rpc_client *cl);

/* Helper function to find the requested function */
function_reg *find_function(char *function_name, function_reg *function_list);

/* Helper function to handle find request */
void handle_rpc_find(int client_sock, char *function_name, const rpc_data *data, function_reg *function_list);

/* Helper function to handle call request */
void handle_rpc_call(int client_sock, char *function_name, rpc_data *data, function_reg *function_list);

/* Helper function to handle a new connection and fulfill request */
void *handle_connection(void *arg);

/* Function to free rpc_data */
void rpc_data_free(rpc_data *data);


#endif //COMP30023_2023_PROJECT_2_RPC_INTERNAL_H
