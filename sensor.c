#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <json-c/json.h>
#include <unistd.h>

#define MAX 80
#define PORT 2424
#define SA struct sockaddr 

void toJson(char* *data, char* out);

int main(int argc, char * argv[]){
    if (argc < 2){
        fprintf(stderr, "Usage: %s <ip address>\n", argv[0]);
        exit(1);
    }

	int sockfd; 
	struct sockaddr_in servaddr, cli; 
    char * buff;
    char *dir = "data/";
    char filename[32], filepath[32+strlen(dir)];
    FILE *fin;

    struct json_object     *json_parsed, *json_cmd, *json_who, *json_content;

	// Socket create and varification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("Unable to create socket.\n"); 
		exit(0); 
	} 
	else
		printf("Socket created.\n"); 
	bzero(&servaddr, sizeof(servaddr)); 

	// Assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(argv[1]); 
	servaddr.sin_port = htons(PORT); 

	// Connect the client socket to server socket 
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
		printf("Unable to connect to server.\n"); 
		exit(0); 
	} 
	else
		printf("Connected to server.\n"); 
    
    int ok = 1;
    while (ok = 1) {
        printf("Enter file name: ");
        scanf("%s", filename);
        sprintf(filepath, "%s%s", dir, filename);
        printf("Path: %s\n", filepath);
		fin = fopen(filepath, "r");
		char str[4];
		size_t len = 0;
		char nama[50];
		printf("\tSensor: ");
		scanf("%s", nama);
		char topic[50];
		printf("\tTopic: ");
		scanf("%s",topic);
		char content[50];
		
		while(fgets(str, sizeof(str), fin) != NULL) {
			int data= atoi(str);
			char buffer[1024];
			sprintf(content, "%d", data);
			char * list[4] = {"pub", nama, topic, content};
			toJson(list, buffer);
			send(sockfd, buffer, sizeof (buffer), 0 );
			sleep((double)1);
			fgets(str, sizeof(str), fin);
		}
		
		fclose(fin);
    }
	
    close(sockfd);
	
    return 0;
}


void toJson(char** data, char* out) {
    char * key[4] = {"command", "who", "topic", "content"};
    char json_str[1024];
    strcpy(json_str, "{");
    int x = 1;
    for (int i = 0; i < 4; i++) {
        char temp[1024];
        sprintf(temp, "\"%s\":\"%s\"", key[i], data[i]);
        strcpy(json_str+x, temp);
        x+=5+strlen(key[i])+strlen(data[i]);
        if (i != 3) {
            strcpy(json_str+x, ",");
            x++;
        }
    }
    strcpy(json_str+x, "}\n");
    strcpy(out, json_str);
}