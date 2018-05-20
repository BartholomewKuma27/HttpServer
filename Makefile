RM      = rm -f

default: http_server

fsh: http_server.c
	gcc -o http_server http_server.c -lpthread

clean veryclean:
	$(RM) http_server
