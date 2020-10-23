all: read_server write_server

read_server: server.c
	gcc server.c -D READ_SERVER -o read_server
write_server: server.c
	gcc server.c -o write_server

clean: 
	rm -r write_server
	rm -r read_server
	rm -r preorderRecord
	cp preorderRecord_cp preorderRecord
