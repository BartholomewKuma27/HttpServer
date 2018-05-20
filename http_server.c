#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h> 
#include <string.h>
#include <time.h>

int MAX_LINE_SIZE = 100;
int CONFIG_ARG_SIZE = 6;
int MAX_SERVERS_NUM = 10;

void*
add_in_host_info(char**, char*, char*);
//v means vhost,d - docroot, c - cgi-bin, i - ip, p - port, l - logs
char first_letters[] = {'v','d','c','i','p','l'};

struct parsed_info{
	char ** host_info;
	char * host_info_container;
};

struct server_args{
	char * server_dir;
	int * sock_ptr;
};

struct rangeT {
	int start, end;
};

void*
start_server(char **, char *, pthread_t *);
void*
execute_server(void *);
void*
serve_client(void *, char **, char*w);
char*
get_requested_file(char *, int);
char* 
concat_strings(char *, char *);
int
file_exists(char *);
int
is_file(char *);
char*
generate_html_body_for_path(char *, char *);
int
file_sending(char *, int, char *, struct rangeT *, char **);
char* 
get_type(char *);
char* 
add_header(char *, char *, char *);
char* 
get_file_header(char *, char *, char *, char *, char *);
char* 
get_static_header(char *, char *, char *, int, char *);
struct rangeT* 
get_range(char *);
FILE* 
log_file(char **);
char*
get_curr_time();
char* 
get_user_agent(char *);
char* 
get_host_ip(char *);
char* 
error_404();
char* 
create_log(char *, char*, char *, char *, char *, char *, char *);
void* 
save_log(int, char **, char *, char*, char *, char *, char *, char *, char *);
char* 
error_log(char *, char *, char *);
int
is_requested_file_cgi(char*);
char*
get_cgi_data(char *);
int 
has_cache_header(char *);
char*
get_cache_hash(char *);
char* 
get_304_header(char *);
char* 
get_file_hash(char *);


int main(int argc, char* argv[]){
	if (argc < 2){
		printf("%s\n", "configfile path needed");
		exit(1);
	}
	char * path_to_configfile = argv[1];
	FILE * file;
	file = fopen(path_to_configfile, "r");
	char line[MAX_LINE_SIZE];
	int ind = 0;
	int a = 1; //kaxa )))
	//es aris array sadac chagdebuli elementebi iqneba mimdevrobit
	//host, docroot, cgi-bin, ip, port, log
	char ** host_info = host_info = malloc(sizeof(char*) * CONFIG_ARG_SIZE);;
	char * host_info_container = host_info_container = malloc(sizeof(char) * CONFIG_ARG_SIZE);
	//array of threads, we need this for joining at the end of main thread
	char ** thread_arr = malloc(sizeof(char*) * MAX_SERVERS_NUM);
	int thread_arr_ind = 0;
	while (1){
		if(fgets(line, MAX_LINE_SIZE, file)){
			if (line[0] != '\n'){
				add_in_host_info(host_info, line, host_info_container);
			}
			else{
				pthread_t tmp_thread;
				start_server(host_info, host_info_container, &tmp_thread);
				break;
				thread_arr[thread_arr_ind++] = (char*) tmp_thread;
				host_info = malloc(sizeof(char*) * CONFIG_ARG_SIZE);
				host_info_container = malloc(sizeof(char) * CONFIG_ARG_SIZE);
			}
		}
		else{
			pthread_t tmp_thread;
			start_server(host_info, host_info_container, &tmp_thread);
			thread_arr[thread_arr_ind++] = (char*) tmp_thread;
			break;
		}
	}
	return 0;
}

void*
start_server(char ** host_info, char* host_info_container, pthread_t * thrd){
	struct parsed_info * p_info = malloc(sizeof(struct parsed_info));
	p_info->host_info = host_info;
	p_info->host_info_container = host_info_container;
	//pthread_t cur_thread;
	//pthread_create(&cur_thread, NULL, execute_server, (void*)p_info);
	execute_server(p_info);
	//*thrd = cur_thread;
}

void*
execute_server(void * arguments){
	struct parsed_info p_info = *((struct parsed_info *)arguments);
	char ** host_info = p_info.host_info;
	char * host_info_container = p_info.host_info_container;

	char * ip = host_info[3];
	int port = atoi(host_info[4]);

	int sock_fd;
	struct sockaddr_in server_addr;
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		perror("Socket open Error");
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &(server_addr.sin_addr.s_addr));
	if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
		perror("socket--bind Error");
	}
	int accepter_limit = 5;
	printf("%s%s%s%i\n", "starting listening ip: ", ip, " port:", port);
	if (listen(sock_fd, accepter_limit) != 0) {
		perror("socket--listen Error");
	}
	while (1){
		int com_sock_fd;
		struct sockaddr_in clinet_addr;
		int client_addr_size = sizeof(clinet_addr);
		printf("%s\n", "velodebi accepts");
		com_sock_fd = accept(sock_fd, (struct sockaddr *)&clinet_addr, &client_addr_size);

		printf("%s\n", "movida accept");
		//pthread_t cur_thread;
		struct server_args * sarg = malloc(sizeof(struct server_args));
		sarg->server_dir = host_info[1];
		sarg->sock_ptr = &com_sock_fd;
		//pthread_create(&cur_thread, NULL, serve_client, (void*)sarg);
		serve_client(sarg, host_info, host_info_container);
	}
	//pthread_join(cur_thread, NULL);
}

void*
serve_client(void * arg, char** host_info, char* host_info_container){
	struct server_args * sargs = (struct server_args *)arg;
	char * server_dir  = sargs->server_dir;
	int com_sock_fd = *(sargs->sock_ptr);
	char * received_data;
	int max_len_for_received_data;
	max_len_for_received_data = 1000;
	received_data = malloc(sizeof(char) * max_len_for_received_data);
	int rs = recv(com_sock_fd, received_data, max_len_for_received_data, 0);
	char * requested_file = get_requested_file(received_data, rs);
	char* status_code = malloc(4);
	int result = 0;
	char* curr_time = get_curr_time();
	char* client = "127.0.0.1";

	if (requested_file == NULL){
		char* error_dat = "Error request received";
		save_log(1, host_info, curr_time, client, error_dat, "", "", "", "");
		close(com_sock_fd);
	} else {
		server_dir = concat_strings(server_dir, "/");
		char * requested_file_full_path;
		int is_cgi = is_requested_file_cgi(requested_file);
		if (is_cgi){
			if(host_info_container[2] == '0'){
				requested_file_full_path = requested_file;
			}
			else{
				requested_file_full_path = concat_strings(host_info[2], requested_file);
			}
		}
		else{
			requested_file_full_path = concat_strings(server_dir, &requested_file[1]);
		}
		char * html_body;
		char * data_for_send;
		int was_file = 0;
		if (file_exists(requested_file_full_path)){
			if(is_file(requested_file_full_path)){
				was_file = 1;
			} else {
				status_code = "200";
				html_body = generate_html_body_for_path(requested_file_full_path, requested_file);
				char* header_start = strdup("HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: ");
				if(html_body == NULL) 
					html_body = "<html><head><title>Gugusha Announce</title></head><body>Empty Folder<html><head><title>Gugusha</title></head><body>";
				char buff[10];
				sprintf(buff, "%d", (int)strlen(html_body));
				char* with_len = concat_strings(header_start, buff);
				with_len = concat_strings(with_len, "\n\n");
				data_for_send = strdup(with_len);
				data_for_send = concat_strings(data_for_send, html_body);
			}
		} else {
			data_for_send = error_404();	
			status_code = "404";
		}
		if (was_file == 1) {
			if (is_cgi){
				char* cgi_data = get_cgi_data(requested_file_full_path);
				send(com_sock_fd, cgi_data, strlen(cgi_data), 0);
				result = (int)strlen(cgi_data);
			}
			else{
				struct rangeT* rn = get_range(received_data);
				result = file_sending(received_data, com_sock_fd, requested_file_full_path, rn, &status_code);	
			}	
		} else{
			rs = send(com_sock_fd, data_for_send, strlen(data_for_send), 0);
			result = (int)strlen(data_for_send);
		}
		char* domain = get_host_ip(received_data);
		char data_len[15];
		sprintf(data_len, "%d", result);
		char* user_agent = get_user_agent(received_data);
		save_log(0, host_info, curr_time, client, domain, requested_file, status_code, data_len, user_agent);
	}
	close(com_sock_fd);
}

char*
get_cgi_data(char* file_full_path){
	printf("%s\n", file_full_path);
	char * final_data;
	int pipes[2];
	pipe(pipes);
	switch(fork()){
		case -1:
			close(pipes[0]);
			close(pipes[1]);
			break;
		case 0:
			close(pipes[0]);
			dup2(pipes[1], 1);
			char * args[1];
			args[0] = NULL;
			execv(file_full_path, args);
			close(pipes[1]);
			break;
		default:
			close(pipes[1]);
			int final_data_size = 0;
			int read_buffer_size = 100;
			char read_buffer[read_buffer_size];
			int ind = 0;
			while(1){
				int bytes_read = read(pipes[0], read_buffer, read_buffer_size);
				if (ind == 0){
					final_data_size = bytes_read + 1;
					final_data = malloc(final_data_size);
					strcpy(final_data, read_buffer);
				}
				else{
					final_data_size += bytes_read;
					final_data = realloc(final_data, final_data_size);
					final_data = concat_strings(final_data, read_buffer);
				}
				if (bytes_read != read_buffer_size){
					break;
				}
				ind += 1;
			}
			close(pipes[0]);
			break;
	}
	return final_data;
}

int
is_requested_file_cgi(char* file){
	char * point_pt = strrchr(file, '.');
	if (point_pt == NULL){
		return 0;
	}
	if (strcmp(point_pt + 1, "cgi") == 0){
		return 1;
	}
	return 0;
}

char* 
get_file_hash(char *path) {
    struct stat attr;
    stat(path, &attr);
    int ctm = (int)attr.st_mtime;
	char time_str[15];
	sprintf(time_str, "%d", ctm);
	int p_len = (int) strlen(path);
	int start = 0;
	char* addition = strdup("");
	if(p_len >= 10)
		start = p_len - 10;
	else {
		int i;
		for(i = 0; i < (10 - p_len); i++)
			addition = concat_strings(addition, "w");
	}
    char* p = path + start;
    p = concat_strings(p, addition); 
    char* tm = concat_strings(time_str, p);
    return tm;
}


int file_sending(char* received_data, int socket, char *file, struct rangeT* rn, char** status) {
	int file_fd;
	struct stat st;
	file_fd = open(file, 0);
	fstat(file_fd, &st);
	int result = st.st_size;
	char b_len[10];
	sprintf(b_len, "%d", result);
	char* ext = get_type(file);
	int has_range = 0;
	if(rn != NULL)
		has_range = 1;
	if(has_range) {
		char fs_len[10];
		sprintf(fs_len, "%d", rn->start);
		if(rn->end == -1)
			rn->end = result - 1;
		char fe_len[10];
		sprintf(fe_len, "%d", rn->end);
		int real_len =  rn->end - rn->start + 1;
		char r_len[10];
		sprintf(r_len, "%d", real_len);
		char* header = get_file_header(ext, b_len, fs_len, fe_len, r_len);
		send(socket, header, strlen(header), 0);
		result = real_len;
		off_t starting_offset = (off_t) rn->start;
		int bytes_to_send = real_len;
		int cur_sent_bytes;
		while(cur_sent_bytes = sendfile(socket, file_fd, &starting_offset, bytes_to_send)){
			bytes_to_send -= cur_sent_bytes;
			if(bytes_to_send <= 0){
				break;
			}
		}
		//sendfile(socket, file_fd, (off_t *)&rn->start, real_len);
		strcpy(*status,"206");
	} else {
		char* NM_header;
		int hash_same = -1;
		char* Jakucuma_hash = get_file_hash(file); 
		if(has_cache_header(received_data) == 1) {
			char* recv_hash = get_cache_hash(received_data);
			hash_same = strcmp(recv_hash, Jakucuma_hash);
			if(hash_same == 0) {
				strcpy(*status, "304");
				NM_header = get_304_header(Jakucuma_hash);
				send(socket, NM_header, strlen(NM_header), 0);
			}
		} 
		if(hash_same != 0) {
			printf("SHEMOVEDI AQ\n\n");
			char* header = get_static_header("HTTP/1.1 200 OK\n", ext, b_len, 1, Jakucuma_hash);
			strcpy(*status, "200");
			send(socket, header, strlen(header), 0);
			sendfile(socket, file_fd, NULL, result);
		}
	}
	return (int)result;
}

char*
generate_html_body_for_path(char* folder_full_path, char* requested_file){
	int start_size = 50;
	char* html_body = malloc(start_size);
	int ind = 0;
	DIR * opened_dir;
	struct dirent * dir;
	opened_dir = opendir(folder_full_path);

	while (1)
	{
	  dir = readdir(opened_dir);
	  if (dir == NULL){
	  	break;
	  }
	  char first_letter = dir->d_name[0];
	  if (first_letter != '.'){
	  	char * new_request_file;
	  	int tmp_len = strlen(requested_file);
	  	char last_letter = requested_file[tmp_len - 1];
	  	if (last_letter != '/'){
	  		char * tmp_slash = strdup("/");
	  		requested_file = concat_strings(requested_file, tmp_slash);
	  	}
	  	new_request_file = concat_strings(requested_file, dir->d_name);
	  	char * part1 = strdup("<a href=");
	  	char * part2 = concat_strings(part1, new_request_file);
	  	char * part3 = concat_strings(part2,"> ");
	  	char * part4 = concat_strings(part3, dir->d_name);
	  	char * part5 = strdup("</a> <br>\n");
	  	char * part6 = concat_strings(part4, part5);
	  	if(ind == 0)
	  		html_body = part6;
	  	else
	  		html_body = concat_strings(html_body, part6);
	  	ind += 1;
	  }
	}
	closedir(opened_dir);
	if (ind == 0)
		return NULL;
	return html_body;
}

int
is_file(char* file_full_path){
	struct stat pt;
    stat(file_full_path, &pt);
    return S_ISREG(pt.st_mode);
}

int
file_exists(char* file_full_path){
	if (access(file_full_path, F_OK) == -1){
	 	return 0;
	}
	return 1;
}

char* 
concat_strings(char* first, char* second){
	int new_size = strlen(first) + strlen(second);
	char* new_word = malloc(sizeof(char) * (new_size + 1));
	strcpy(new_word, first);
	strcat(new_word, second);
	return new_word;
}

char*
get_requested_file(char* data, int data_len){
	int i;
	int first_index = -1;
	int second_index = -1;
	for (i = 0; i < data_len; ++i)
	{
		if (data[i] == ' '){
			if (first_index == -1){
				first_index = i;
			}
			else{
				second_index = i;
				break;
			}
		}
	}
	if(first_index == second_index){
		return NULL;
	}
	int file_name_size = second_index - first_index - 1;
	char* file_name = malloc(sizeof(char) * (file_name_size + 1 ));
	memcpy(file_name, &data[first_index+1], file_name_size);
	memcpy(&(file_name[file_name_size]),"\0",1);
	return file_name;
}

void *
add_in_host_info(char** arr, char* elem, char* container){
	char * key;
	char * val;
	int i;
	int is_val_empty = 1;
	for (i = 0; i < strlen(elem); ++i)
	{
		char ch = elem[i];
		if (ch == '='){
			int diff = i - 0;
			int key_size = diff-1; //1 char spacia
			key = malloc(sizeof(char) * key_size);
			strncpy(key, elem, key_size);
			int value_size = strlen(elem) - i - 3; //1 char aqac spaceia
			if (value_size > 0 ){
				val = malloc(sizeof(char) * value_size);
				strncpy(val, elem + diff + 2, value_size); //+2 imito ro erti space da erti '=' da -2 imito ro /n/n boloshi movashorot
				is_val_empty = 0; //anu value arsebobs
			}
			break;
		}
	}
	char first_letter = elem[0];
	i = 0;
	for (i = 0; i < CONFIG_ARG_SIZE; ++i)
	{
		if (first_letter == first_letters[i]){
			if (is_val_empty == 1){
				container[i] = '0';
			}
			else{
				arr[i] = val;
				container[i] = '1';
				
			}
			break;
		}
	}
}

char*
add_header(char* header, char* key, char* value) {
	key = concat_strings(key, ": ");
	key = concat_strings(key, value);
	key = concat_strings(key, "\n");
	header = concat_strings(header, key);
	return header;
}

char* get_str_between(char* start, char* end) {
	size_t len = end - start;
	char* result = (char*)malloc(sizeof(char)*(len));
	strncpy(result, start, len - 1);
	result[len] = '\0';
	return result;	
}

char* 
get_host_ip(char * header) {
	char* u_agent_start = strcasestr(header, "Host:");
	char* u_agent_end = strcasestr(u_agent_start, "\n");
	char* result = get_str_between(u_agent_start, u_agent_end);
	return result + 5;
}

char* 
get_user_agent(char * header) {
	char* u_agent_start = strcasestr(header, "User-Agent:");
	char* u_agent_end = strcasestr(u_agent_start, "\n");
	char* result = get_str_between(u_agent_start, u_agent_end);
	return result + 12;
}

char* 
get_static_header(char* status, char* type, char* len, int finish, char* MD5hash) {
	char* header;
	header = strdup(status);
	header = add_header(header, "Content-Type", type);
	header = add_header(header, "Content-Length", len);
	if(MD5hash != NULL) {
		header = add_header(header, "Cache-Control", "max-age=5");
		// davageneriro HASH da chavwero etag-shi
		header = concat_strings(header, "ETag: \"");
		header = concat_strings(header, MD5hash);
		header = concat_strings(header, "\"\n");
	}
	if(finish)
		header = concat_strings(header, "\n");
	return header;
}

char* 
get_file_header(char* type, char* len, char* f_start, char* f_end, char* real_len) {
	char* header;
	header = get_static_header("HTTP/1.1 206 Partial Content\n", type, real_len, 0, NULL);
	header = concat_strings(header, "Accept-Ranges: bytes");
	header = concat_strings(header, "\nContent-Range: bytes ");
	header = concat_strings(header, f_start);
	header = concat_strings(header, "-");
	header = concat_strings(header, f_end);
	header = concat_strings(header, "/");
	header = concat_strings(header, len);
	header = concat_strings(header, "\n\n");
	return header;
}

char* 
get_304_header(char* hash) {
	char* header;
	header = strdup("HTTP/1.1 304 Not Modified\nCache-Control: max-age=5\nETag: \"");
	header = concat_strings(header, hash);
	header = concat_strings(header, "\"\n\n");
	return header;
}

FILE* 
log_file(char** host_info) {
	char* logname = host_info[5];
	FILE* logs = fopen(logname, "a+");
	return logs;
}

char*
get_curr_time() {
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	char* tm = asctime(timeinfo);
	tm[strlen(tm)-1] = '\0';
	return tm;
}

char* 
get_type(char* path) {
	char* ext;
	int len = strlen(path);
	while(1) {
		char* curr_ch = path + len -1;
		if(curr_ch[0] == '.') {
			ext = path + (len - 1);
			break;
		}
		len--;
	}
	if(strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0) {
		ext = concat_strings("image/", ext+1);
	} else if(strcmp(ext, ".mp4") == 0) {
		ext = concat_strings("video/", ext+1);
	} else 
		ext = concat_strings("text/", ext+1);
	return ext;
}

struct rangeT* 
get_range(char* header) {
	char* rn = strdup("range");
	char* st = strcasestr(header, rn); 
	if(st != NULL) {
		st = st + 7;
		int i = 0;
		while(1) {
			char ch = st[i];
			if(ch == '-')
				break;
			i++;
		}
		char* start = malloc(i+1);
		memcpy(start, st+6, i);
		start[i] = '\0';
		st = st + i + 1;
		i = 0;
		while(1) {
			char ch = st[i];
			if(ch == '\n')
				break;
			i++;
		}
		char* end = malloc(i+1);
		memcpy(end, st, i);
		end[i] = '\0';
		struct rangeT* range = malloc(sizeof(struct rangeT*));
		range->start = atoi(start);
		if(i != 1)
			range->end = atoi(end);
		else
			range->end = -1;
		return range;
	}
	return NULL;
}

char* 
error_404() {
	char* html = strdup("HTTP/1.1 404 Not Found\nnContent-Type: text/html\n\n<html><head><title>Gugusha Announcer</title></head><body>ERROR 404</body></html>");
	return html;
}

char* 
create_log(char* date, char* client, char* domain, char* file, char* status, char* len, char* user_agent) {
	char* output = "";
	output = concat_strings(output, "[");
	output = concat_strings(output, date);
	output = concat_strings(output, "] ");
	output = concat_strings(output, client);
	output = concat_strings(output, " ");
	output = concat_strings(output, domain);
	output = concat_strings(output, " ");
	output = concat_strings(output, file);
	output = concat_strings(output, " ");
	output = concat_strings(output, status);
	output = concat_strings(output, " ");
	output = concat_strings(output, len);
	output = concat_strings(output, " \"");
	output = concat_strings(output, user_agent);
	output = concat_strings(output, "\"");
	return output;
}

char* 
error_log(char* date, char* client, char* err_dat) {
	char* output = "";
	output = concat_strings(output, "[");
	output = concat_strings(output, date);
	output = concat_strings(output, "] ");
	output = concat_strings(output, client);
	output = concat_strings(output, " \"");
	output = concat_strings(output, err_dat);
	output = concat_strings(output, "\"");
	return output;
}
 
void* 
save_log(int is_error, char** host_info, char* date, char* client, char* domain, char* file, char* status, char* len, char* user_agent) {
	char* logdata;
	if(is_error == 1) {
		logdata = error_log(date, client, domain);
	} else {
		logdata = create_log(date, client, domain, file, status, len, user_agent);
	}
	char* logname = host_info[5];
	FILE* logs = fopen(logname, "a+");
	fprintf(logs, "%s\n", logdata);
	fclose(logs);
}

int 
has_cache_header(char* header) {
	if(strcasestr(header, "If-None-Match:") == NULL)
		return 0;
	return 1;
}

char*
get_cache_hash(char* header) {
	char* str = strcasestr(header, "If-None-Match:");
	char* end = strcasestr(str, "\n");
	char* result = get_str_between(str, end);
	result = result + 16;
	result[strlen(result)-1] = '\0';
	return result;
}
