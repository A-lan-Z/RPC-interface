#include "rpc.h"
#include "rpc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>


/* Function to initialize server */
rpc_server *rpc_init_server(int port) {
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
        perror("bind");
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
    // Return failure if any of the arguments is NULL or name is empty
    if (srv == NULL || name == NULL || strlen(name) < 1 || handler == NULL) {
        return -1;
    }

    // Check if a function with the same name is already registered
    function_reg *existing_function = find_function(name, srv->registered_functions);
    if (existing_function != NULL) {
        // If a function with the same name is already registered, overwrite its handler
        existing_function->handler = handler;
        return 1;
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
    return 1;
}

/* Function to start the server */
void rpc_serve_all(rpc_server *srv) {
    // Return if srv is NULL
    if (srv == NULL) {
        return;
    }

    // Listen for incoming connections
    listen(srv->server_sock, 5);

    // Accept incoming connections in an infinite loop
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
