#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define RPC_ERROR 0
#define RPC_SUCCESS 1
#define RPC_FIND 2
#define RPC_CALL 3


/* Using a linked list to store all the registered function for the server */
typedef struct function_reg {
    char *function_name;
    rpc_data* (*handler)(rpc_data*);
    struct function_reg *next;
} function_reg;


struct connection_args {
    rpc_server *srv;
    int client_sock;
};

struct rpc_server {
    int server_sock;
    struct function_reg *registered_functions;
    int is_running;
};

struct rpc_handle {
    char *function_name;
};

/* Helper function to send message using designed protocol */
int rpc_send_message(int sock, int operation, char *name, rpc_data *data) {
    fprintf(stderr, "working rpc_send_message starting to socket %d\n", sock);
    fprintf(stderr, "working rpc_send_message operation: %d,   name: %s\n", operation, name);
    if (data && data->data1) {
        fprintf(stderr, "working rpc_send_message returning result: %d\n", data->data1);
    }

    // Convert ints to network byte order
    uint32_t name_len_net = htonl(strlen(name));
    uint32_t data_len_net = data ? htonl(data->data2_len) : 0;
    uint32_t data1_net = data ? htonl((uint32_t)data->data1) : 0;

    // Send header
    fprintf(stderr, "working rpc_send_message sending %d\n", sock);
    write(sock, &operation, sizeof(operation));
    write(sock, &name_len_net, sizeof(name_len_net));
    write(sock, &data_len_net, sizeof(data_len_net));
    fprintf(stderr, "working rpc_send_message sent header\n");

    // Send function name
    write(sock, name, strlen(name));
    fprintf(stderr, "working rpc_send_message sent function name\n");

    if (data) {
        // Send rpc_data
        write(sock, &data1_net, sizeof(data1_net));
        write(sock, data->data2, data->data2_len);
        fprintf(stderr, "working rpc_send_message sent data\n");
    }

    return 0;
}

function_reg *find_function(char *function_name, function_reg *function_list) {
    fprintf(stderr, "working find_function\n");
    function_reg *current = function_list;
    while (current!= NULL) {
        fprintf(stderr, "find_function Current function name: %s\n", current->function_name);
        fprintf(stderr, "find_function Looking for: %s\n", function_name);

        if (strcmp(current->function_name, function_name) == 0) {
            fprintf(stderr, "working find_function found\n");
            return current;
        }
        current = current->next;
    }
    fprintf(stderr, "working find_function not found\n");
    return NULL;
}


void handle_rpc_find(int client_sock, char *function_name, const rpc_data *data, function_reg *function_list) {
    fprintf(stderr, "working handle_rpc_find\n");
    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    fprintf(stderr, "working handle_rpc_find find function returned: %s\n", func->function_name);
    if (func == NULL) {
        // Send an error response to the client
        fprintf(stderr, "working handle_rpc_find error response\n");

        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        return;
    }
    fprintf(stderr, "working handle_rpc_find success response\n");

    // Send a success response to the client
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, NULL);
    fprintf(stderr, "working handle_rpc_find message sent\n");

}

void handle_rpc_call(int client_sock, char *function_name, rpc_data *data, function_reg *function_list) {
    fprintf(stderr, "working handle_rpc_call\n");

    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    if (func == NULL) {
        fprintf(stderr, "working handle_rpc_call function not found\n");
        // Send an error response to the client
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        return;
    }
    // Call the function
    rpc_data *output_data = func->handler(data);
    fprintf(stderr, "working handle_rpc_call function called, result: %d\n", output_data->data1);
    // Send a response to the client with the output data
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, output_data);
    fprintf(stderr, "working handle_rpc_call reply sent\n");
    // Free the output data
    rpc_data_free(output_data);
}



/* Helper function to receive message using designed protocol */
void *handle_connection(void *arg) {
    fprintf(stderr, "working handle_connection\n");
    struct connection_args *args = arg;
    rpc_server *srv = args->srv;
    int client_sock = args->client_sock;

    // Read the operation code, function name length, and data length from the client
    int operation;
    uint32_t name_len_net, data_len_net;
    read(client_sock, &operation, sizeof(operation));
    read(client_sock, &name_len_net, sizeof(name_len_net));
    read(client_sock, &data_len_net, sizeof(data_len_net));

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);

    // Read the function name and data from the client
    char *function_name = malloc(name_len + 1);
    read(client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    rpc_data *data = malloc(sizeof(rpc_data));
    uint32_t data1_net;
    data->data1 = 0;
    data->data2_len = data_len;
    data->data2 = malloc(data_len);
    read(client_sock, &data1_net, sizeof(data1_net));
    data->data1 = ntohl(data1_net);
    read(client_sock, data->data2, data_len);
    fprintf(stderr, "working handle_connection message read\n");
    // Handle the operation
    switch (operation) {
        case RPC_FIND:
            fprintf(stderr, "working handle_connection find request found\n");
            handle_rpc_find(client_sock, function_name, data, srv->registered_functions);
            fprintf(stderr, "working handle_connection find request terminated\n");

            break;
        case RPC_CALL:
            fprintf(stderr, "working handle_connection call request found\n");
            handle_rpc_call(client_sock, function_name, data, srv->registered_functions);
            break;
    }
    fprintf(stderr, "working handle_connection operation handled\n");

    // Clean up
    free(function_name);
    free(data->data2);
    free(data);
    close(client_sock);
    free(arg);
    fprintf(stderr, "handle_connection exiting+++++++++++++++++++++++++++++++++++++++\n");
    return NULL;
}




rpc_server *rpc_init_server(int port) {
    // Attempt to allocate memory
    rpc_server *server = malloc(sizeof(rpc_server));
    if (server == NULL) {
        return NULL;
    }

    // Attempt to allocate socket
    server->server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server->server_sock < 0) {
        free(server);
        return NULL;
    }

    // Set up server details
    struct sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);
    server_addr.sin6_addr = in6addr_any; // listen on all interfaces

    int enable = 1;
    if (setsockopt(server->server_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the server address and port
    if (bind(server->server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server->server_sock);
        free(server);
        return NULL;
    }

    server->registered_functions = NULL;
    server->is_running = 1;
    return server;
}


int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    // Return failure if any of the arguments is NULL
    if (srv == NULL || name == NULL || handler == NULL) {
        return -1;
    }

    // Attempt to allocate memory
    function_reg *new_function = malloc((sizeof(function_reg)));
    if (new_function == NULL) {
        return -1;
    }

    // Register the function
    new_function->function_name = strdup(name);
    new_function->handler = handler;
    new_function->next = srv->registered_functions;
    srv->registered_functions = new_function;
    printf("%s registered\n", srv->registered_functions->function_name);
    return 1;
}

void rpc_serve_all(rpc_server *srv) {
    // Listen for incoming connections
    listen(srv->server_sock, 5);

    while (srv->is_running) {
        fprintf(stderr, "working serving all\n");
        // Accept a new connection
        int client_sock = accept(srv->server_sock, NULL, NULL);
        if (client_sock < 0) {
            continue;
        }
        fprintf(stderr, "working connection found\n");

        // Handle the connection in a new thread
        struct connection_args *args = malloc(sizeof(struct connection_args));
        args->srv = srv;
        args->client_sock = client_sock;
        pthread_t thread;
        fprintf(stderr, "working connection established\n");
        pthread_create(&thread, NULL, handle_connection, args);
        pthread_detach(thread); // Detach the thread
    }
}


struct rpc_client {
    int client_sock;
    int is_connected;
    struct sockaddr_in6 server_addr;
};



rpc_client *rpc_init_client(char *addr, int port) {
    fprintf(stderr, "working rpc_init_client\n");

    // Attempt to allocate memory
    rpc_client *client = malloc(sizeof(rpc_client));
    if (client == NULL) {
        return NULL;
    }

    // Attempt to allocate socket
    client->client_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (client->client_sock < 0) {
        free(client);
        return NULL;
    }

    // Set up server details
    struct sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);
    if (inet_pton(AF_INET6, addr, &server_addr.sin6_addr) <= 0) {
        free(client);
        return NULL;
    }
    fprintf(stderr, "working rpc_init_client\n");

//    // Connect to the server
//    if (connect(client->client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//        free(client);
//        fprintf(stderr, "working rpc_init_client failed\n");
//
//        return NULL;
//    }
    fprintf(stderr, "working rpc_init_client\n");

    // Store the server address and port
    client->server_addr = server_addr;
    client->is_connected = 1;
    return client;
}

/* Right now this function assumes that everything can fit in one packet */
rpc_handle *rpc_find(rpc_client *cl, char *name) {
    // Connect to the server
    if (connect(cl->client_sock, (struct sockaddr *)&cl->server_addr, sizeof(cl->server_addr)) < 0) {
        free(cl);
        fprintf(stderr, "working rpc_init_client failed\n");

        return NULL;
    }
    fprintf(stderr, "new socket for find request created--------------------------------------------\n");

    rpc_data data = {0, 0, NULL};
    fprintf(stderr, "working rpc_find\n");

    // Send rpc_find message
    rpc_send_message(cl->client_sock, RPC_FIND, name, &data);
    fprintf(stderr, "working rpc_find message sent\n");

    // Receive response
    int operation;
    uint32_t name_len_net, data_len_net;
    read(cl->client_sock, &operation, sizeof(operation));
    read(cl->client_sock, &name_len_net, sizeof(name_len_net));
    read(cl->client_sock, &data_len_net, sizeof(data_len_net));

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);

    // Check operation code for error
    if (operation == RPC_ERROR) {
        return NULL;
    }
    fprintf(stderr, "working rpc_find message received\n");

    // Read function name
    char *function_name = malloc(name_len + 1);
    read(cl->client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    // Create rpc_handle
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    handle->function_name = function_name;
    fprintf(stderr, "working rpc_find handled created\n");

    fprintf(stderr, "working rpc_find operation: %d   name_len: %d   data_len: %d\n", operation, name_len, data_len);
    // Read output data
    uint32_t data1_net;
    void *discard_buffer = malloc(data_len);
    read(cl->client_sock, &data1_net, sizeof(data1_net));
    if (data_len > 0) {
        read(cl->client_sock, discard_buffer, data_len);
    }
    fprintf(stderr, "working rpc_find finishing\n");

    return handle;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // Connect to the server
    if (connect(cl->client_sock, (struct sockaddr *)&cl->server_addr, sizeof(cl->server_addr)) < 0) {
        free(cl);
        fprintf(stderr, "working rpc_init_client failed\n");

        return NULL;
    }
    fprintf(stderr, "new socket for call request created--------------------------------------------\n");

    fprintf(stderr, "working rpc_call\n");
    // Return NULL if any of the arguments is NULL
    if (cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }

    // Send rpc_call message
    fprintf(stderr, "working rpc_call sending message\n");
    rpc_send_message(cl->client_sock, RPC_CALL, h->function_name, payload);
    fprintf(stderr, "working rpc_call message sent\n");

    // Receive response
    int operation;
    uint32_t name_len_net, data_len_net;
    read(cl->client_sock, &operation, sizeof(operation));
    read(cl->client_sock, &name_len_net, sizeof(name_len_net));
    read(cl->client_sock, &data_len_net, sizeof(data_len_net));
    fprintf(stderr, "working rpc_call here1\n");

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);
    fprintf(stderr, "working rpc_call here2\n");

    // Check operation code for error
    if (operation == RPC_ERROR) {
        fprintf(stderr, "working rpc_call here3 %d\n", operation);
        return NULL;
    }

    // Allocate new rpc_data structure
    rpc_data *output_data = malloc(sizeof(rpc_data));
    output_data->data2_len = data_len;
    fprintf(stderr, "working rpc_call here4\n");


    // Read function name
    char *function_name = malloc(name_len + 1);
    read(cl->client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    // Read output data
    uint32_t data1_net;
    read(cl->client_sock, &data1_net, sizeof(data1_net));
    output_data->data1 = ntohl(data1_net);
    if (data_len > 0) {
        output_data->data2 = malloc(data_len);
        read(cl->client_sock, output_data->data2, data_len);
    }
    fprintf(stderr, "working rpc_call response received: result:  %d, data2_len:  %d\n", output_data->data1, data_len);

    return output_data;
}

void rpc_close_client(rpc_client *cl) {
    if (cl == NULL) {
        return;
    }

    // Close the client socket
    close(cl->client_sock);

    // Free the client struct
    free(cl);
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

