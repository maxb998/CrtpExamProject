all: actor_customers producer server

actor_customers: actor_customers.c
	gcc actor_customers.c -o actor_customers

producer: producer.c
	gcc producer.c -o producer

server: server.c
	gcc server.c -o server


clean: 
	rm actor_customers producer server