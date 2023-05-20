CC=cc
RPC_SYSTEM=rpc.o

.PHONY: format all

all: $(RPC_SYSTEM) rpc-server rpc-client

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) -c -o $@ $<

rpc-server: server.c $(RPC_SYSTEM)
	$(CC) -o $@ $^

rpc-client: client.c $(RPC_SYSTEM)
	$(CC) -o $@ $^

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
# 	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

format:
	clang-format -style=file -i *.c *.h
