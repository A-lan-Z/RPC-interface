#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct rpc_server {
    int server_sock;
    struct function_reg * registered_functions;
    int is_running
};

/* Using a linked list to store all the registered function for the server */
struct function_registry {
    char *function_name;
    rpc_data* (*handler)(rpc_data*);
    struct function_registry *next;
};


rpc_server *rpc_init_server(int port) {
    // Attempt to allocate memory
    rpc_server *server = malloc(sizeof(rpc_server));
    if (server == NULL) {
        return NULL;
    }
    // Attempt to allocate socket
    server->server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_sock < 0) {
        free(server);
        return NULL;
    }

    server->registered_functions = NULL;
    server->is_running = 0;
    return server;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    return -1;
}

void rpc_serve_all(rpc_server *srv) {

}

struct rpc_client {
    int client_sock;
    int is_connected;
};

struct rpc_handle {
    char *function_name;
    rpc_data* (*handler)(rpc_data*);
};

rpc_client *rpc_init_client(char *addr, int port) {
    // Attempt to allocate memory
    rpc_client *client = malloc(sizeof(rpc_client));
    if (client == NULL) {
        return NULL;
    }
    // Attempt to allocate socket
    client->client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->client_sock < 0) {
        free(client);
        return NULL;
    }

    client->is_connected = 0;
    return client;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    return NULL;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    return NULL;
}

void rpc_close_client(rpc_client *cl) {

}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
