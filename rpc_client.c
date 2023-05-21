#include "rpc.h"
#include "rpc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


/* Function to initialize client */
rpc_client *rpc_init_client(char *addr, int port) {
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
    // Return NULL if any of the arguments is NULL
    if (cl == NULL || name == NULL) {
        return NULL;
    }

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
    return handle;
}

/* Function to create a new connection and send a call request to the server */
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // Return NULL if any of the arguments is NULL
    if (cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }

    // Check if data2_len is too large to be encoded in the packet format
    if (payload->data2_len > 100000) {
        fprintf(stderr, "Overlength error\n");
        return NULL;
    }

    // Return NULL if data2_len doesn't match the actual size of data2
    if (payload->data2 == NULL && payload->data2_len != 0) {
        return NULL;
    } else if (payload->data2 != NULL && payload->data2_len == 0) {
        return NULL;
    }

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
    uint64_t data1_net;
    read(client_sock, &data1_net, sizeof(data1_net));
    output_data->data1 = ntohll(data1_net);
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