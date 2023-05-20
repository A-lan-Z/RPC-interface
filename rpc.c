#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define NONBLOCKING
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

struct rpc_client {
    int is_connected;
    struct sockaddr_in6 server_addr;
};


/* Helper function to send message using designed protocol */
int rpc_send_message(int sock, int operation, char *name, rpc_data *data) {
    fprintf(stderr, "rpc_send_message: operation: %d, function: %s\n", operation, name);
    // Convert ints to network byte order
    uint32_t name_len_net = htonl(strlen(name));
    uint32_t data_len_net = data ? htonl(data->data2_len) : 0;
    uint32_t data1_net = data ? htonl((uint32_t)data->data1) : 0;

    // Send header
    write(sock, &operation, sizeof(operation));
    write(sock, &name_len_net, sizeof(name_len_net));
    write(sock, &data_len_net, sizeof(data_len_net));

    // Send function name
    write(sock, name, strlen(name));

    if (data) {
        // Send rpc_data
        write(sock, &data1_net, sizeof(data1_net));
        write(sock, data->data2, data->data2_len);
    }
    fprintf(stderr, "rpc_send_message: operation: %d, function: %s   Message sent\n", operation, name);

    return 0;
}

/* Helper function to find the requested function */
function_reg *find_function(char *function_name, function_reg *function_list) {
    fprintf(stderr, "find_function: function name: %s\n", function_name);
    function_reg *current = function_list;

    // Iterate through list of functions to find requested function
    while (current!= NULL) {
        if (strcmp(current->function_name, function_name) == 0) {
            fprintf(stderr, "find_function: function name: %s  function found\n", function_name);
            return current;
        }
        current = current->next;
    }
    // Function not found
    fprintf(stderr, "find_function: function name: %s  function NOT found\n", function_name);
    return NULL;
}

/* Helper function to handle find request */
void handle_rpc_find(int client_sock, char *function_name, const rpc_data *data, function_reg *function_list) {
    fprintf(stderr, "handle_rpc_find: function name: %s\n", function_name);
    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    if (func == NULL) {
        // Function not found, send an error response to the client
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        fprintf(stderr, "handle_rpc_find: function name: %s   sent error response\n", function_name);

        return;
    }

    // Function found, send a success response to the client
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, NULL);
    fprintf(stderr, "handle_rpc_find: function name: %s   sent success response\n", function_name);

}

/* Helper function to handle call request */
void handle_rpc_call(int client_sock, char *function_name, rpc_data *data, function_reg *function_list) {
    fprintf(stderr, "handle_rpc_call: function name: %s\n", function_name);
    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    if (func == NULL) {
        // Function not found, send an error response to the client
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        fprintf(stderr, "handle_rpc_call: function name: %s   sent error response\n", function_name);

        return;
    }

    // Function found, call the function
    rpc_data *output_data = func->handler(data);
    fprintf(stderr, "handle_rpc_call: function name: %s   function called\n", function_name);

    // Send a response to the client with the output data
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, output_data);
    rpc_data_free(output_data);
    fprintf(stderr, "handle_rpc_call: function name: %s   sent success response\n", function_name);

}

/* Helper function to handle a new connection and fulfill request */
void *handle_connection(void *arg) {
    fprintf(stderr, "handle_connection\n");
    struct connection_args *args = arg;
    rpc_server *srv = args->srv;
    int client_sock = args->client_sock;

    // Read the header: operation code, function name length, and data length from the client
    int operation;
    uint32_t name_len_net, data_len_net;
    read(client_sock, &operation, sizeof(operation));
    read(client_sock, &name_len_net, sizeof(name_len_net));
    read(client_sock, &data_len_net, sizeof(data_len_net));

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);

    // Read the function name from the client
    char *function_name = malloc(name_len + 1);
    if (function_name == NULL) {
        perror("malloc");
        return NULL;
    }
    read(client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    // Read the rpc data from the client
    rpc_data *data = malloc(sizeof(rpc_data));
    if (data == NULL) {
        perror("malloc");
        free(function_name);
        return NULL;
    }
    uint32_t data1_net;
    data->data1 = 0;
    data->data2_len = data_len;
    data->data2 = malloc(data_len);
    if (data->data2 == NULL && data_len > 0) {
        perror("malloc");
        free(function_name);
        free(data);
        return NULL;
    }
    read(client_sock, &data1_net, sizeof(data1_net));
    data->data1 = ntohl(data1_net);
    read(client_sock, data->data2, data_len);

    // Handle the operation
    switch (operation) {
        case RPC_FIND:
            handle_rpc_find(client_sock, function_name, data, srv->registered_functions);
            break;
        case RPC_CALL:
            handle_rpc_call(client_sock, function_name, data, srv->registered_functions);
            break;
        default:
            free(function_name);
            free(data->data2);
            free(data);
            close(client_sock);
            free(arg);
            return NULL;
    }

    // Clean up and close client
    free(function_name);
    free(data->data2);
    free(data);
    close(client_sock);
    free(arg);
    fprintf(stderr, "handle_connection complete\n");

    return NULL;
}

/* Function to initialize server */
rpc_server *rpc_init_server(int port) {
    fprintf(stderr, "rpc_init_server");
    // Attempt to allocate memory
    rpc_server *server = malloc(sizeof(rpc_server));
    if (server == NULL) {
        perror("malloc");
        return NULL;
    }

    // Attempt to allocate socket
    server->server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server->server_sock < 0) {
        perror("socket");
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

/* Function to register the server functions */
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    fprintf(stderr, "rpc_register: registering function %s\n", name);
    // Return failure if any of the arguments is NULL
    if (srv == NULL || name == NULL || handler == NULL) {
        return -1;
    }

    // Attempt to allocate memory
    function_reg *new_function = malloc((sizeof(function_reg)));
    if (new_function == NULL) {
        perror("malloc");
        return -1;
    }

    // Register the function
    new_function->function_name = strdup(name);
    if (new_function->function_name == NULL) {
        perror("strdup");
        free(new_function);
        return -1;
    }
    new_function->handler = handler;
    new_function->next = srv->registered_functions;
    srv->registered_functions = new_function;
    fprintf(stderr, "rpc_register: function registered %s\n", name);

    return 1;
}

/* Function to start the server */
void rpc_serve_all(rpc_server *srv) {
    fprintf(stderr, "rpc_serve_all");
    // Listen for incoming connections
    listen(srv->server_sock, 5);

    // Accept incoming connections in a infinite loop
    while (srv->is_running) {
        int client_sock = accept(srv->server_sock, NULL, NULL);
        if (client_sock < 0) {
            continue;
        }

        // Handle the connection in a new thread
        struct connection_args *args = malloc(sizeof(struct connection_args));
        if (args == NULL) {
            perror("malloc");
            close(client_sock);
            continue;
        }
        args->srv = srv;
        args->client_sock = client_sock;
        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, args);
        pthread_detach(thread); // Detach the thread
    }
}

/* Function to initialize client */
rpc_client *rpc_init_client(char *addr, int port) {
    fprintf(stderr, "rpc_init_client");
    // Attempt to allocate memory
    rpc_client *client = malloc(sizeof(rpc_client));
    if (client == NULL) {
        perror("malloc");
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

    // Store the server details
    client->server_addr = server_addr;
    client->is_connected = 1;
    return client;
}

/* Function to create a new connection and send a find request to the server */
rpc_handle *rpc_find(rpc_client *cl, char *name) {
    fprintf(stderr, "rpc_find: %s\n", name);
    // Create a new socket
    int client_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Connect to the server
    if (connect(client_sock, (struct sockaddr *)&cl->server_addr, sizeof(cl->server_addr)) < 0) {
        free(cl);
        return NULL;
    }

    // Send rpc_find message
    rpc_data data = {0, 0, NULL};
    rpc_send_message(client_sock, RPC_FIND, name, &data);

    // Receive response
    int operation;
    uint32_t name_len_net, data_len_net;
    read(client_sock, &operation, sizeof(operation));
    read(client_sock, &name_len_net, sizeof(name_len_net));
    read(client_sock, &data_len_net, sizeof(data_len_net));

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);

    // Check operation code for error
    if (operation == RPC_ERROR) {
        fprintf(stderr, "rpc_find: %s   function Not found\n", name);
        return NULL;
    }

    // Read function name
    char *function_name = malloc(name_len + 1);
    if (function_name == NULL) {
        perror("malloc");
        return NULL;
    }
    read(client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    // Create rpc_handle
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    if (handle == NULL) {
        perror("malloc");
        free(function_name);
        return NULL;
    }
    handle->function_name = function_name;

    // Read output data
    uint32_t data1_net;
    void *discard_buffer = malloc(data_len);
    read(client_sock, &data1_net, sizeof(data1_net));
    if (data_len > 0) {
        read(client_sock, discard_buffer, data_len);
    }
    fprintf(stderr, "rpc_find: %s   function found, returning handle to %s\n", name, handle->function_name);

    return handle;
}

/* Function to create a new connection and send a call request to the server */
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    fprintf(stderr, "rpc_call: %s\n", h->function_name);
    // Create a new socket
    int client_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Connect to the server
    if (connect(client_sock, (struct sockaddr *)&cl->server_addr, sizeof(cl->server_addr)) < 0) {
        free(cl);
        return NULL;
    }

    // Return NULL if any of the arguments is NULL
    if (cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }

    // Send rpc_call message
    rpc_send_message(client_sock, RPC_CALL, h->function_name, payload);

    // Receive response
    int operation;
    uint32_t name_len_net, data_len_net;
    read(client_sock, &operation, sizeof(operation));
    read(client_sock, &name_len_net, sizeof(name_len_net));
    read(client_sock, &data_len_net, sizeof(data_len_net));

    // Convert lengths to host byte order
    size_t name_len = ntohl(name_len_net);
    size_t data_len = ntohl(data_len_net);

    // Check operation code for error
    if (operation == RPC_ERROR) {
        fprintf(stderr, "rpc_call: %s   function NOT found\n", h->function_name);
        return NULL;
    }

    // Allocate new rpc_data structure
    rpc_data *output_data = malloc(sizeof(rpc_data));
    if (output_data == NULL) {
        perror("malloc");
        return NULL;
    }
    output_data->data2_len = data_len;

    // Read function name
    char *function_name = malloc(name_len + 1);
    read(client_sock, function_name, name_len);
    function_name[name_len] = '\0'; // null-terminate the string

    // Read output data
    uint32_t data1_net;
    read(client_sock, &data1_net, sizeof(data1_net));
    output_data->data1 = ntohl(data1_net);
    if (data_len > 0) {
        output_data->data2 = malloc(data_len);
        if (output_data->data2 == NULL) {
            perror("malloc");
            free(output_data);
            return NULL;
        }
        read(client_sock, output_data->data2, data_len);
    } else {
        output_data->data2 = NULL;
    }
    fprintf(stderr, "rpc_call: %s   function returned data1 = %d,  data2_len = %d\n", h->function_name, output_data->data1, output_data->data2_len);
    return output_data;
}

/* Function to close client */
void rpc_close_client(rpc_client *cl) {
    if (cl == NULL) {
        return;
    }
    // Free the client struct
    free(cl);
}

/* Function to free rpc_data */
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}

