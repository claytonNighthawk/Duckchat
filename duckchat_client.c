/* CIS 432 Computer Networks Fall 2017
 * University of Oregon
 * Ashton Shears and Clayton Kilmer
 * 
 * This program is a chat client over UDP
 */
#include <ctype.h>
#include "duckchat.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "pthread.h"
#include <arpa/inet.h>
#include "raw.c"
/*Global Vars*/
int clientSocket;
int maxUDPbytes = 65535;
char current_channel_list[CHANNEL_MAX][CHANNEL_MAX];
int n_channels;
int is_running;
char current_channel[CHANNEL_MAX];
pthread_t sender_thread;
pthread_t reciever_thread;

struct sockaddr_in clientaddr;
struct sockaddr_in serveraddr;

struct request_login *login_rq;
struct request_logout *logout_rq;
struct request_join *join_rq;
struct request_leave *leave_rq;
struct request_say *say_rq;
struct request_list *list_rq;
struct request_who *who_rq;

struct text *gen_text;
struct text_say *say_text;
struct text_list *list_text;
struct text_who *who_text;
struct text_error *error_text;

/*Helper Function*/
int find_channel(char* target){
    int i;
    for (i = 0; i < n_channels; i++){
        if (strcmp(current_channel_list[i],target) == 0)
            return i;        
    }
    return -1;
}
/*Sender Thread*/
void *sender(){
    char string[256]; /*Line input*/
    //int strlen = 0;
    //char c;
    while(is_running){
       /*
        c = '\0';
        while (c != '\n'){
            c = getchar();
            if (c == '\n'){
             //   for (int i = 0; i < strlen; i++)
               //     printf("\b");
                //fflush(stdout);
                break;
            }
            putchar(c);
            string[strlen] = c;
            strlen++;
        }*/
        //string[strlen + 1] = '\n';
        //strlen = 0;
        //c = '0';
        scanf(" %[^\n]s", string);//read input
        /*Client Commands*/
        if (string[0] == '/'){
            /*Send exit request*/
            if(strcmp("/exit", string) == 0){
                sendto(clientSocket, logout_rq, sizeof(struct request_logout), 0, (struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in));
                cooked_mode();
                is_running = 0;
                exit(0);
            }/*Send join request*/
            else if (strncmp("/join ", string, 6) == 0){
                if (string[7] == '\0' || string[6] == '\0')
                    puts("Must enter a valid server name");
                else{
                    strcpy(join_rq->req_channel, &string[6]);//changed
                    if (sendto(clientSocket, join_rq, sizeof(struct request_join),0,(struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in))){
                        strcpy(current_channel, join_rq->req_channel);
                        n_channels++;
                        strcpy(current_channel_list[n_channels - 1], current_channel);
                    }
                }
            }/*Send leave request*/
            else if (strncmp("/leave ", string, 7) == 0){
                int j;
                if ((j = find_channel(&string[7])) != -1){
                    strcpy(leave_rq->req_channel, &string[7]);
                    if (sendto(clientSocket, leave_rq, sizeof(struct request_leave),0,(struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in)))
                        strcpy(current_channel_list[j], "666");
                    else
                        puts("Error sending leave request to the server");
                }
                else
                    puts("Not currently subscribed to that channel");
            }/*Send list request*/
            else if (strcmp("/list", string) == 0){
                if (!sendto(clientSocket, list_rq, sizeof(struct request_list),0,(struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in)))
                    puts("Error sending list request to the server");
            }/*Send who request*/
            else if (strncmp("/who ", string, 5) == 0){
                strcpy(who_rq->req_channel, &string[5]);
                if (!sendto(clientSocket, who_rq, sizeof(struct request_who),0, (struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in)))
                    puts("Error sending who request to the server");   
            }/*Switch current Channel*/   
            else if (strncmp("/switch ", string, 8) == 0){
                printf("searching for channel: %s\n", &string[8]);
                if (find_channel(&string[8]) != -1){
                    strcpy(current_channel, &string[8]);
                    printf("current channel is now: %s\n", current_channel);
                }
                else
                    puts("Non-valid server.\n");
            }
            else
                puts("Invalid command");
        }
        else{/*Send a Say message*/
            strcpy(say_rq->req_text, string);
            strcpy(say_rq->req_channel, current_channel);
            if (!sendto(clientSocket, say_rq, sizeof(struct request_say),0,(struct sockaddr*)&serveraddr, sizeof(struct sockaddr_in)))
                puts("Error sending say message to server"); 
        }
        memset(string, 0 ,256);
    }
    return 0;
}
/*Reciever Thread*/
void *reciever(){
    while(is_running){
        recvfrom(clientSocket,gen_text,maxUDPbytes,0, NULL, NULL);
        /*If server sent a say message*/
        if (gen_text->txt_type == TXT_SAY){
            say_text = (struct text_say*) gen_text; 
            printf("[%s]", say_text->txt_channel);
            printf("[%s]", say_text->txt_username);
            printf(": %s\n", say_text->txt_text);
        }/*If server sent a list message*/
	    if (gen_text->txt_type == TXT_LIST){
            list_text = (struct text_list*) gen_text;
            puts("Existing channels:\n");
            for (int i = 0; i < list_text->txt_nchannels; i++)
                printf(" %s\n", list_text->txt_channels[i].ch_channel);
        }/*If server sent a who message*/
  	    if (gen_text->txt_type == TXT_WHO){
	        who_text = (struct text_who*) gen_text;
            printf("Users on channel %s:\n", who_text->txt_channel);
            for (int i = 0; i < who_text->txt_nusernames; i++)
                printf(" %s\n", who_text->txt_users[i].us_username);
        }/*If server sent an error message*/
	    if (gen_text->txt_type == TXT_ERROR){
	        error_text = (struct text_error*) gen_text;
            printf("Error: %s\n", error_text->txt_error);
        }
    }
    return 0;
}
/*Start threads function*/
void create_threads(){
    pthread_create(&sender_thread, NULL, sender, NULL);
    pthread_create(&reciever_thread, NULL, reciever, NULL);
}
/*Prepares client socket and server socket address*/
void init_client(char *server_ip, int server_port){
    /*Client address information*/
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = htonl(0);
    clientaddr.sin_port = htons(0);

    /*Server address information*/
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(server_port);

    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0){
        puts("Could not create socket");
        exit(1);
    }
    if (bind(clientSocket,(struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0){
        puts("Bind socket error");
        exit(1);
    } 
    /*Initialize requests that can be sent to server*/
    login_rq = malloc(sizeof(struct request_login));
    login_rq->req_type = REQ_LOGIN;
    logout_rq = malloc(sizeof(struct request_logout));
    logout_rq->req_type = REQ_LOGOUT;
    join_rq = malloc(sizeof(struct request_join));
    join_rq->req_type = REQ_JOIN;
    leave_rq = malloc(sizeof(struct request_leave));
    leave_rq->req_type = REQ_LEAVE;
    say_rq = malloc(sizeof(struct request_say));
    say_rq->req_type = REQ_SAY;
    list_rq = malloc(sizeof(struct request_list));
    list_rq->req_type = REQ_LIST;
    who_rq = malloc(sizeof(struct request_who));
    who_rq->req_type = REQ_WHO;
    /*Initialize structures which can be recieved from the server*/
    gen_text = malloc(sizeof(struct text));
    say_text = malloc(sizeof(struct text_say));
    list_text = malloc(sizeof(struct text_list));
    who_text = malloc(sizeof(struct text_who));
    error_text = malloc(sizeof(struct text_error));

    create_threads();
}

int main(int argc, char **argv){
    if (argc != 4){
        puts("Duckchat client requires exactly three command-line arguments");
        return 0;
    }
    is_running = 1;
    n_channels = 1;
    raw_mode();
    init_client(argv[1], atoi(argv[2]));

    /*Send login request to server*/
    strcpy(login_rq->req_username, argv[3]);
    if (sendto(clientSocket, login_rq, sizeof(struct request_login),0,(struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in)))
        printf("Welcome to Duckchat %s, enjoy your stay.\n", argv[3]);
    else{
        puts("Unable to send login request to server. Abort.");
        exit(0);
    }
    /*Join Common channel on login*/
    strcpy(join_rq->req_channel, "Common");
    if (!sendto(clientSocket, join_rq, sizeof(struct request_join),0,(struct sockaddr*) &serveraddr, sizeof(struct sockaddr_in))){
        puts("Unable to send join request to server. Abort.");
        exit(0);
    }
    strcpy(current_channel, "Common");
    strcpy(current_channel_list[n_channels - 1], current_channel);
    pthread_exit(NULL);
    return 0;
}
