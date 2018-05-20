#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){
	char* text = (char*)strdup("CGI Test Done Succesfully");
	int text_len = (int)strlen(text);
	printf("%s", "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: ");
	printf("%i", text_len);
	printf("%s", "\n\n");
	printf("%s", text);
}