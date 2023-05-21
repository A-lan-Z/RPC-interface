#include "rpc.h"
#include "rpc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>


/* Helper function to convert 8-byte integer to network byte order */
uint64_t htonll(uint64_t value) {
    // Check the endianness of the system
    const int num = 1;
    if(*(char *)&num == 1) {
        // If the system is little endian
        const uint32_t high_part = htonl((uint32_t)(value >> 32));
        const uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));

        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        // If the system is big endian
        return value;
    }
}

/* Helper function to convert network byte order to 8-byte integer */
uint64_t ntohll(uint64_t value) {
    // Check the endianness of the system
    const int num = 1;
    if(*(char *)&num == 1) {
        // If the system is little endian
        const uint32_t high_part = ntohl((uint32_t)(value >> 32));
        const uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));

        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        // If the system is big endian
        return value;
    }
}

/* Helper function to send message using designed protocol */
int rpc_send_message(int sock, int operation, char *name, rpc_data *data) {
    // Convert ints to network byte order
    uint32_t name_len_net = htonl(strlen(name));
    uint32_t data_len_net = data ? htonl(data->data2_len) : 0;
    uint64_t data1_net = data ? htonll((uint64_t)data->data1) : 0;

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
    return 0;
}

/* Helper function to find the requested function */
function_reg *find_function(char *function_name, function_reg *function_list) {
    function_reg *current = function_list;

    // Iterate through list of functions to find requested function
    while (current!= NULL) {
        if (strcmp(current->function_name, function_name) == 0) {
            return current;
        }
        current = current->next;
    }
    // Function not found
    return NULL;
}

/* Helper function to handle find request */
void handle_rpc_find(int client_sock, char *function_name, const rpc_data *data, function_reg *function_list) {
    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    if (func == NULL) {
        // Function not found, send an error response to the client
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        return;
    }

    // Function found, send a success response to the client
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, NULL);
}

/* Helper function to handle call request */
void handle_rpc_call(int client_sock, char *function_name, rpc_data *data, function_reg *function_list) {
    // Check if the function is registered
    function_reg *func = find_function(function_name, function_list);
    if (func == NULL) {
        // Function not found, send an error response to the client
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        return;
    }

    // Function found, call the function
    rpc_data *output_data = func->handler(data);

    if (output_data == NULL) {
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        return;
    } else if ((output_data->data2 == NULL && output_data->data2_len != 0) ||
               (output_data->data2 != NULL && output_data->data2_len == 0)) {
        // Send error response if data2_len doesn't match the actual size of data2
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        rpc_data_free(output_data);
        return;
    }

    // Check if data2_len is too large to be encoded in the packet format
    if  (output_data->data2_len > 100000) {
        fprintf(stderr, "Overlength error\n");
        rpc_send_message(client_sock, RPC_ERROR, "", NULL);
        rpc_data_free(output_data);
        return;
    }

    // Send a response to the client with the output data
    rpc_send_message(client_sock, RPC_SUCCESS, function_name, output_data);
    rpc_data_free(output_data);
}

/* Helper function to handle a new connection and fulfill request */
void *handle_connection(void *arg) {
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
    data->data1 = 0;
    data->data2_len = data_len;
    data->data2 = malloc(data_len);
    if (data->data2 == NULL && data_len > 0) {
        perror("malloc");
        free(function_name);
        free(data);
        return NULL;
    }

    uint64_t data1_net;
    read(client_sock, &data1_net, sizeof(data1_net));
    data->data1 = ntohll(data1_net);
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
    return NULL;
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