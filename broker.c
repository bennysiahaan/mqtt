#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <errno.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h>

/* Json library */
#include <json-c/json.h>

#define TRUE 1 
#define FALSE 0 
#define SA struct sockaddr
#define PORT 2424

#define SERVER_OK 		"200" 
#define SENSOR_OK 		"400" 
#define SENSOR_INIT		"401"
#define SENSOR_PUBLISH	"402"
#define CLIENT_OK 		"600" 
#define CLIENT_INIT 	"601"
#define CLIENT_GET 		"602"

#define MAX_STR 32

struct client
{
	char id[MAX_STR];
	int sockfd;
	char topic[MAX_STR];
	int condition;
};

int addtoList(struct client *in,struct client *out);
int deleteList(int sock, struct client *id_in);
void toJson(char** data, char* out);
int n_client = 0;

int main(int argc , char *argv[]){ 
	if (argc < 2){
        fprintf(stderr, "Usage: %s <number of subscribers>\n", argv[0]);
        exit(1);
    }
	int max_subs = atoi(argv[1]);
	int max_clients = 2*max_subs;
	struct client list_client[max_subs];

	for (int x=0; x<max_subs; x++){
		list_client[x].condition = 0;
	}
	int opt = TRUE; 
	int master_socket, addrlen, new_socket, client_socket[10] , 
		activity, i, valread, sd; 
	int max_sd; 
	struct sockaddr_in address; 
		
	char buffer[1024] = {0};
		
	// Set of socket descriptors 
	fd_set readfds; 
			
	// Initialize all client_socket[] to 0 
	for(i = 0; i < max_clients; i++){ 
		client_socket[i] = 0; 
	} 
		
	// Create a master socket
	if( (master_socket = socket(AF_INET, SOCK_STREAM , 0)) == 0){ 
		perror("Unable to create master socket.\n"); 
		exit(EXIT_FAILURE); 
	} 
	
	// Set master socket to allow multiple connections
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){ 
		perror("setsockopt failed.\n"); 
		exit(EXIT_FAILURE); 
	}
	
	// Type of socket created 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT );
		
	// Bind the socket to localhost port 8888
	if (bind(master_socket, (SA *)&address, sizeof(address))<0){ 
		perror("Unable to bind to socket.\n"); 
		exit(EXIT_FAILURE); 
	} 
	printf("Listener on port %d for %d client and %d Subscriber\n", PORT, max_clients, max_subs); 
		
	// Try to specify maximum of 5 pending connections for the master socket 
	if (listen(master_socket, 5) < 0){ 
		perror("listen failed.\n"); 
		exit(EXIT_FAILURE); 
	} 
		
	// Accept the incoming connection 
	addrlen = sizeof(address); 
	puts("Waiting for connections...\n"); 

	while(TRUE){ 
		// Clear the socket set 
		FD_ZERO(&readfds); 
	
		// Add master socket to set 
		FD_SET(master_socket, &readfds); 
		max_sd = master_socket; 
			
		// Add child sockets to set 
		for ( i = 0 ; i < max_clients ; i++){ 
			// Socket descriptor 
			sd = client_socket[i]; 
				
			// If socket descriptor is valid then add to read list 
			if(sd > 0) 
				FD_SET( sd , &readfds); 
				
			// Highest file descriptor number for the select function 
			if(sd > max_sd)
				max_sd = sd;
		} 
	
		// Wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely 
		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL); 
	
		if ((activity < 0) && (errno!=EINTR)){ 
			printf("select failed.\n"); 
		} 
			
		// If something happened on the master socket, then it is an incoming connection 
		if (FD_ISSET(master_socket, &readfds)){ 
			if ((new_socket = accept(master_socket, (SA *)&address, (socklen_t*)&addrlen))<0){ 
				perror("accept failed.\n"); 
				exit(EXIT_FAILURE);
			} 
			
			// Inform user of socket number - used in send and receive commands 
			printf("Connected. [#Socket: %d, IP: %s, Port: %d]\n", 
            	new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
			
			// Add new socket to array of sockets 
			for (i = 0; i < max_clients; i++){ 
				// If position is empty 
				if( client_socket[i] == 0 ){ 
					client_socket[i] = new_socket;
					break; 
				} 
			} 
		} 
			
		// Else it is some IO operation on some other socket
		for (i = 0; i < max_clients; i++){ 
			sd = client_socket[i]; 
				
			if (FD_ISSET( sd , &readfds)){ 
				// Check if it was for closing, and also read the incoming message 
				if ((valread = read( sd , buffer, 1024)) == 0){ 
					// Somebody got disconnected, get his details and print
					getpeername(sd , (SA*)&address , (socklen_t*)&addrlen); 
						
					for (int i=0; i<5; i++){
						if (deleteList(sd, &list_client[i])){
							printf("\t%s disconnected. [#Socket: %d]\n",
								list_client[i].id, list_client[i].sockfd);
							break;
						}
					}
					// Close the socket and mark as 0 in list for reuse
					close( sd ); 
					client_socket[i] = 0; 
				} 
				// Process data
				else {
					struct json_object *parsed_json;
					struct json_object *cmd;
					struct json_object *id_client;
					struct json_object *topic_json;
					struct json_object *content_json;

					parsed_json = json_tokener_parse(buffer);

					json_object_object_get_ex(parsed_json, "command", &cmd);
					json_object_object_get_ex(parsed_json, "who", &id_client);
					json_object_object_get_ex(parsed_json, "topic", &topic_json);
					json_object_object_get_ex(parsed_json, "content", &content_json);
					
					char command[MAX_STR]; 
					char client_id[MAX_STR];
					char topic[MAX_STR];
					char content[MAX_STR];
					sprintf(command, "%s", json_object_get_string(cmd));
					sprintf(client_id, "%s", json_object_get_string(id_client));
					sprintf(topic, "%s", json_object_get_string(topic_json));
					sprintf(content, "%s", json_object_get_string(content_json));

					struct client tmp;
					sprintf(tmp.id, "%s", client_id);
					sprintf(tmp.topic, "%s", topic);
					tmp.sockfd = sd;
					if (strcmp(command, "sub") == 0){
						if (strcmp(content, "init") == 0){
							if (n_client == max_subs){
								char *temp2 = "Unable to load any more subscribers.";
								printf("%s\n", temp2);
								send(sd , temp2 , strlen(temp2) , 0 );
							} else {
								for (int i=0; i<5; i++){
									if (addtoList(&tmp, &list_client[i]) > 0){
										printf("\t%s connected [#Socket: %d, Topic: %s]\n", 
											list_client[i].id, list_client[i].sockfd, list_client[i].topic);
										char *temp2 = "Client listed.\n";
										send(sd , temp2 , strlen(temp2) , 0 );
										n_client++;
										break;
									} 
								}
							}
						}
					}

					if (strcmp(command, "pub") == 0){
						if (n_client > 0){
							for (int a = 0; a<5; a++){
								if (strcmp(list_client[a].topic, topic) == 0){
									memset(buffer, 0, sizeof(buffer));
									char *data_out[4]={"pub", client_id, list_client[a].topic, content};
									toJson(data_out, buffer);
									send(list_client[a].sockfd , buffer , strlen(buffer) , 0 );
								}
							}
						}
					}
				} 
			} 
		} 
	} 
		
	return 0; 
} 

int addtoList(struct client *in, struct client *out){
	int dapat=0;
	if (out->condition == 0){
		sprintf(out->id, "%s", in->id);
		out->sockfd = in->sockfd;
		sprintf(out->topic, "%s", in->topic);
		out->condition = 1;
		dapat=1;
	}
	return dapat;
}

int deleteList(int sock, struct client *id_in){
	int stat = 0;
	if (id_in->sockfd == sock){
		id_in->condition = 0;
		n_client--;
		stat = 1;
	}
	return stat;
}

void toJson(char** data, char* out){
	const int l = 4;
    char * key[4] = {"command", "who", "topic", "content"};
    char json_str[1024];
    strcpy(json_str, "[");
    int x = 1;
    for (int i=0; i<l; i++){
        char temp[1024];
        sprintf(temp, "\"%s\":\"%s\"", key[i], data[i]);
        strcpy(json_str+x, temp);
        x+=5+strlen(key[i])+strlen(data[i]);
        if (i!=l-1){
            strcpy(json_str+x, ",");
            x++;
        }
    }
    strcpy(json_str+x, "]\n");
    strcpy(out, json_str);
}