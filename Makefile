CC=cc
CFLAGS=-Wall
LDFLAGS=-pthread
RPC_SYSTEM=rpc.o

.PHONY: format all clean

all: $(RPC_SYSTEM) rpc-server rpc-client

rpc_server.o: rpc_server.c rpc_internal.h
	$(CC) $(CFLAGS) -c -o $@ $<

rpc_client.o: rpc_client.c rpc_internal.h
	$(CC) $(CFLAGS) -c -o $@ $<

rpc_internal.o: rpc_internal.c rpc_internal.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(RPC_SYSTEM): rpc_server.o rpc_client.o rpc_internal.o
	ld -r -o $@ $^

rpc-server: server.c $(RPC_SYSTEM)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

rpc-client: client.c $(RPC_SYSTEM)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f *.o rpc-server rpc-client
