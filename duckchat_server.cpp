/* CIS 432 Computer Networks Fall 2017
 * University of Oregon
 * Ashton Shears and Clayton Kilmer
 * 
 * This program is a chat server over UDP, each server can inter-connect to multiple servers. 
 */
#include "duckchat.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h> 
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>

#include <utility>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <random>

using namespace std;
map<string, pair<string, sockaddr_in>> userIpPorts; // map of user ip:ports to a pair of user names and sockaddr_in structs 
map<string, set<string>> channelUserIpPorts; // map of channel names to set of users (recorded by their ip:port numbers) subbed to that channel 

set<unsigned int> recentlySeenMsgIDs;
map<string, sockaddr_in> adjServers; // map of server ip:ports to sockaddr_in structs that represents the adjectent servers
set<string> thisServerChannels; // all channels this server is subbed to because of clients or to pass to other servers.

// map of channel names to a map of server ip:port strings to not in danger boolean for the soft join of servers on channels.
// the keys of the outter map denote the servers that are subbed to that channel.
// this will not include this server's subscribbed channels as that will be included in thisServerChannels.
map<string, map<string, bool>> channelSubbedServers;
sockaddr_in myAddress;
int serverSocket;

// init request structs
request* gen_request = new request(); 
request_login* login_request = new request_login(); 
request_join* join_request = new request_join(); 
request_leave* leave_request = new request_leave(); 
request_say* say_request = new request_say();
request_who* who_request = new request_who(); 

// init text structs
text_say* say_text = new text_say(); 
text_error* error_text = new text_error(); 
text_list* list_text = NULL; // have to be dynamically allocated
text_who* who_text = NULL; // have to be dynamically allocated

// init s2s structs
s2s_join* join_s2s = new s2s_join();
s2s_leave* leave_s2s = new s2s_leave();
s2s_say* say_s2s = new s2s_say();

// Gets a pseudo-random number from urandom file
unsigned long long get_random_num() {
    // guidance taken from https://stackoverflow.com/questions/28115724/getting-big-random-numbers-in-c-c
    unsigned long long randval;
    FILE *f;

    f = fopen("/dev/urandom", "rb");
    fread(&randval, sizeof(randval), 1, f);
    fclose(f);

    mt19937_64 gen(randval);
    uniform_int_distribution<unsigned long long> dis((unsigned long long)0, ULLONG_MAX);

    return dis(gen);
}

void closeEmptyChannels() {
    for (auto it = channelUserIpPorts.begin(); it != channelUserIpPorts.end(); it++) {
        if ((it->second).size() == 0) {
            channelUserIpPorts.erase(it->first);
        }
    }
}

string makeIP_Port_Str(sockaddr_in src_address) {
    stringstream ipStream;
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(src_address.sin_addr), ipAddress, INET_ADDRSTRLEN);

    string ipPort = ipAddress;
    ipPort += ':';
    ipStream << ipPort << ntohs(src_address.sin_port);
    ipStream >> ipPort;
    return ipPort;
}

string hostnameToIP(char* hostname) {
    // hostname to ip from from binarytides.com/hostname-to-ip-address-c-sockets-linux
    struct hostent* he;
    struct in_addr** addr_list;
         
    if ((he = gethostbyname(hostname)) == NULL) {
        perror("Could not get hostname\n");
        return NULL;
    }
 
    addr_list = (in_addr**)he->h_addr_list;
     
    for(int i = 0; addr_list[i] != NULL; i++) {
        //Return the first one;
        string ipString = inet_ntoa(*addr_list[i]);
        return ipString;
    }
    return NULL;
}

void parseArgs(int argc, char** argv) {
    string ipPort;
    int serverPort = atoi(argv[2]);
    myAddress.sin_family = AF_INET;
    myAddress.sin_port = htons(serverPort);
    inet_pton(AF_INET, hostnameToIP(argv[1]).c_str(), &myAddress.sin_addr);
    cout << "myAddress: " << makeIP_Port_Str(myAddress) << endl;

    // populate adjServers with the commandline arguments. 
    int index = 3;
    while (index < argc){
        sockaddr_in newServerAddr;
        newServerAddr.sin_family = AF_INET;
        inet_pton(AF_INET, hostnameToIP(argv[index]).c_str(), &newServerAddr.sin_addr);
        newServerAddr.sin_port = htons(atoi(argv[index+1]));

        ipPort = makeIP_Port_Str(newServerAddr);
        index += 2;
        // cout << "adjServer: " << ipPort << endl;

        memcpy((void*)&(adjServers[ipPort]), (void*)&newServerAddr, sizeof(sockaddr_in));
    }
}

void send_s2s_join_to_all_adj_serv_for_all_my_channels(){
    // For all my channels
    join_s2s->req_type = S2S_JOIN;
    for (auto it = thisServerChannels.begin(); it!= thisServerChannels.end(); it++) {
        strcpy(join_s2s->req_channel, (*it).c_str());
        // For all my adjacent servers
        for (auto it2 = adjServers.begin(); it2 != adjServers.end(); it2++) {
            sendto(serverSocket, join_s2s, sizeof(s2s_join), 0, (sockaddr*)&(it2->second), sizeof(sockaddr_in));
            channelSubbedServers[join_s2s->req_channel][makeIP_Port_Str(it2->second)] = true;
            cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(it2->second) << "  S2S Join " << join_s2s->req_channel << ", resubscribing" << endl;
        }
    }
}

// check if no join has been sent to a subscription. leave if so
void check_for_sent_join() {
    // For all channels
    for (auto it = channelSubbedServers.begin(); it != channelSubbedServers.end(); it++) {
        // For every server listed as subscribed to this channel
        for ( auto it2 = (it->second).begin(); it2 != (it->second).end(); it2++) {
            if (!(it2->second)) {
                (it->second).erase((it2->first));
            } else if (it2->second) {
                it2->second = false;
            }
        }
    }
}

// void cleanUp() {
//     // printf("cleaning up\n");
//     delete gen_request;  
//     delete login_request; 
//     delete join_request; 
//     delete leave_request; 
//     delete say_request;
//     delete who_request; 

//     delete say_text; 
//     delete error_text; 

//     if (list_text != NULL) {
//         // delete[] list_text->txt_channels;
//         delete list_text; 
//     }

//     if (who_text != NULL) {
//         // delete[] who_text->txt_users;
//         delete who_text; 
//     }

//     delete join_s2s;
//     delete leave_s2s;
//     delete say_s2s; 
// }

void sigHandler(int signo) {
    if (signo == SIGINT) {
        // cleanUp();
        exit(0);
    }
    send_s2s_join_to_all_adj_serv_for_all_my_channels();
    check_for_sent_join();
}

void handleLogin(sockaddr_in src_address);
void handleLogout(string ipPort);
void handleJoin(string ipPort);
void handleLeave(string ipPort);
void handleSay(sockaddr_in src_address);
void handleList(sockaddr_in src_address);
void handleWho(sockaddr_in src_address);

void handleS2SJoin(string ipPort);
void handleS2SLeave(string ipPort);
void handleS2SSay(sockaddr_in src_address);

int main(int argc, char **argv) {
    if ((argc - 1) % 2 != 0 || argc == 1){
        perror("Duckchat server requires an even number of server IP port commands as well as this servers IP port.\n");
        return 1;
    }
    stringstream errorStream;
    parseArgs(argc, argv);

    serverSocket = socket(AF_INET, SOCK_DGRAM, 0); //CREATE IPV4 UDP SOCKET
    if (serverSocket < 0) {
        perror("Could not create socket\n");
        return -1;
    }

    if (bind(serverSocket, (sockaddr*)&myAddress, sizeof(myAddress)) != 0) {
        perror("Bind failed!\n");
        return -1;
    }

    socklen_t fromlen = 0;
    sockaddr_in src_address;

    signal(SIGALRM, &sigHandler);
    signal(SIGINT, &sigHandler);
    struct itimerval it_val;
    it_val.it_value.tv_sec = 60;
    it_val.it_value.tv_usec = 0;
    it_val.it_interval = it_val.it_value;
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("TIMER FAILURE");
    }

    while(1) {
        memset(gen_request, 0, sizeof(request));
        memset(login_request, 0, sizeof(request_login));
        memset(join_request, 0, sizeof(request_who));
        memset(leave_request, 0, sizeof(request_leave));
        memset(say_request, 0, sizeof(request_say));
        memset(who_request, 0, sizeof(request_who));
        memset(say_text, 0, sizeof(text_say));
        memset(error_text, 0, sizeof(text_error));
        memset(join_s2s, 0, sizeof(s2s_join));
        memset(leave_s2s, 0, sizeof(s2s_leave));
        memset(say_s2s, 0, sizeof(s2s_say));

        say_text->txt_type = TXT_SAY;
        error_text->txt_type = TXT_ERROR;

        if (recvfrom(serverSocket, gen_request, sizeof(s2s_say), 0, (sockaddr*)&src_address, &fromlen) < 0) {
            cout << "recvfrom failed, continuing." << endl;
            continue;
        }
        // cout << makeIP_Port_Str(myAddress) << " req " <<  gen_request->req_type << endl;

        stringstream errorStream;
        string ipPort = makeIP_Port_Str(src_address);

        if (userIpPorts.count(ipPort) == 0 && adjServers.count(ipPort) == 0 && gen_request->req_type != REQ_LOGIN) { // if ipPort is not a know user or server and the request type is not login send 
            cout << "ERROR! User @ " << ipPort << " has not logged in yet!" << endl;                                 // error message to client and print error to console
            errorStream << "User @ " << ipPort << " has not logged in yet!";
            strcpy(error_text->txt_error, errorStream.str().c_str());
            sendto(serverSocket, error_text, sizeof(text_error), 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
        }
 
        switch (gen_request->req_type) {
            case REQ_LOGIN:
                handleLogin(src_address);
                break;
            case REQ_LOGOUT:
                handleLogout(ipPort);
                break;
            case REQ_JOIN:
                handleJoin(ipPort);
                break;
            case REQ_LEAVE:
                handleLeave(ipPort);
                break;
            case REQ_SAY:
                handleSay(src_address);
                break;
            case REQ_LIST:
                handleList(src_address);
                break;
            case REQ_WHO:
                handleWho(src_address);
                break;
            case S2S_JOIN:
                handleS2SJoin(ipPort);
                break;
            case S2S_LEAVE:
                handleS2SLeave(ipPort);
                break;
            case S2S_SAY:
                handleS2SSay(src_address);
                break;
            default:
                cout << "ERROR! Unknown request type." << endl;
                errorStream << "Unknown request type.";
                strcpy(error_text->txt_error, errorStream.str().c_str());
                sendto(serverSocket, error_text, sizeof(error_text), 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
        }
    }
}

void handleLogin(sockaddr_in src_address) {
    login_request = (request_login*)gen_request;
    string ipPort = makeIP_Port_Str(src_address);
    cout << makeIP_Port_Str(myAddress) << " <- " << ipPort << " REQ login " << login_request->req_username << endl;

    userIpPorts[ipPort].first = string(login_request->req_username);
    memcpy((void*)&(userIpPorts[ipPort].second), (void*)&src_address, sizeof(sockaddr_in));
}

void handleLogout(string ipPort) {
    cout << userIpPorts[ipPort].first << " @ " << ipPort << " logging out." << endl;

    // for each channel remove the userIpPort from the set of userIpPorts subscribed to the channel
    for (auto it = channelUserIpPorts.begin(); it != channelUserIpPorts.end(); it++) {
        it->second.erase(ipPort);
    }
    userIpPorts.erase(ipPort);
    closeEmptyChannels();
}

void handleJoin(string ipPort) {
    join_request = (request_join*)gen_request;
    cout << makeIP_Port_Str(myAddress) << " <- " << ipPort << " REQ Join " << join_request->req_channel << " " << userIpPorts[ipPort].first << endl;

    if (thisServerChannels.count(join_s2s->req_channel) == 0) {
        strcpy(join_s2s->req_channel, join_request->req_channel);
        join_s2s->req_type = S2S_JOIN;
        // send joins to all adjServers and log that those servers are subscribed to that channel
        for (auto it = adjServers.begin(); it != adjServers.end(); it++) {
            sendto(serverSocket, join_s2s, sizeof(s2s_join), 0, (sockaddr*)&(it->second), sizeof(sockaddr_in));
            (channelSubbedServers[join_request->req_channel])[it->first] = true;
            cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(it->second) << "  S2S Join " << join_request->req_channel << endl;
        }
        thisServerChannels.insert(join_request->req_channel);
    }
    channelUserIpPorts[join_request->req_channel].insert(ipPort);
}

void handleLeave(string ipPort) {
    leave_request = (request_leave*)gen_request;
    cout << makeIP_Port_Str(myAddress) << " " << userIpPorts[ipPort].first << " @ " << ipPort << " leaves " << leave_request->req_channel << endl;
    channelUserIpPorts[leave_request->req_channel].erase(ipPort);

    if (channelUserIpPorts[leave_request->req_channel].size() == 0) {
        channelUserIpPorts.erase(leave_request->req_channel);
    }
}

void handleSay(sockaddr_in src_address) {
    say_request = (request_say*)gen_request;

    string ipPort = makeIP_Port_Str(src_address);
    string errorString = "";
    stringstream errorStream;
    
    auto it = channelUserIpPorts.find(say_request->req_channel);
    if ((it != channelUserIpPorts.end() && (it->second).find(ipPort) != (it->second).end()) || adjServers.count(ipPort) != 0) { // if channel exists and user subscribed to it
        strcpy(say_text->txt_channel, say_request->req_channel);
        strcpy(say_text->txt_username, (userIpPorts[ipPort].first).c_str());
        strcpy(say_text->txt_text, say_request->req_text);
        cout << makeIP_Port_Str(myAddress) << " <- " << ipPort << " REQ [" << say_text->txt_channel << "]" << "[" << say_text->txt_username << "] " << say_text->txt_text << endl;

        // for each user subscribed to say_request->req_channel send say_text to. 
        set<string> channelUsers = channelUserIpPorts[say_request->req_channel];
        for (auto it = channelUsers.begin(); it != channelUsers.end(); it++) {
                sendto(serverSocket, say_text, sizeof(text_say), 0, (sockaddr*)&(userIpPorts[*it].second), sizeof(sockaddr_in));
        }

        // for all adjServers that are subscribed to the channel the say is on send an s2s_say to them.
        if (channelSubbedServers.count(say_text->txt_channel) != 0) {
            say_s2s->req_type = S2S_SAY;
            strcpy(say_s2s->req_channel, say_text->txt_channel);
            strcpy(say_s2s->req_username, say_text->txt_username);
            strcpy(say_s2s->txt_field, say_text->txt_text);
            say_s2s->unique_id = get_random_num();
            recentlySeenMsgIDs.insert(say_s2s->unique_id);

            map<string, bool> subscribedServers = channelSubbedServers[say_text->txt_channel]; 
            for (auto it = subscribedServers.begin(); it != subscribedServers.end(); it++){
                sendto(serverSocket, say_s2s, sizeof(s2s_say), 0, (sockaddr*)&(adjServers[it->first]), sizeof(sockaddr_in));
                cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(adjServers[it->first]) << "  S2S [" << say_s2s->req_channel << "]" << "[" << say_s2s->req_username << "] " << say_s2s->txt_field << endl;
            }
        }
    } else {
        cout << "ERROR! Channel " << say_request->req_channel << " does not exist or user has not subscribed to channel!" << endl;
        errorStream << "Channel " << say_request->req_channel << " does not exist or user has not subscribed to channel!"; 
        errorString = errorStream.str();               
        strcpy(error_text->txt_error, errorString.c_str());
        sendto(serverSocket, error_text, sizeof(error_text), 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
    } 
}

void handleList(sockaddr_in src_address) {
    int tlSize = sizeof(text_list) + (channelUserIpPorts.size() * sizeof(channel_info));
    list_text = (text_list*)realloc(list_text, tlSize);
    list_text->txt_type = TXT_LIST;
    list_text->txt_nchannels = channelUserIpPorts.size();

    int i = 0;
    cout << "/list: ";
    for (auto it = channelUserIpPorts.begin(); it != channelUserIpPorts.end(); it++) {
        strcpy(list_text->txt_channels[i].ch_channel, it->first.c_str());
        cout << list_text->txt_channels[i].ch_channel << ", ";             
        i++;
    }
    cout << endl;
    sendto(serverSocket, list_text, tlSize, 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
}

void handleWho(sockaddr_in src_address) {
    who_request = (request_who*)gen_request;

    auto it = channelUserIpPorts.find(who_request->req_channel);
    if (it != channelUserIpPorts.end()) {
        set<string> channelUsers = it->second;

        int twSize = sizeof(text_who) + (channelUsers.size() * sizeof(user_info));
        who_text = (text_who*)realloc(who_text, twSize); 
        who_text->txt_type = TXT_WHO;
        who_text-> txt_nusernames = channelUsers.size();
        strcpy(who_text->txt_channel, who_request->req_channel);

        int i = 0;
        cout << "/who " << who_request->req_channel << ": ";
        for (auto it = channelUsers.begin(); it != channelUsers.end(); it++) {
            strcpy(who_text->txt_users[i].us_username, (userIpPorts[*it].first).c_str());
            cout << who_text->txt_users[i].us_username << ", ";
            i++;
        }
        cout << endl;
        sendto(serverSocket, who_text, twSize, 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
    }
}

void handleS2SJoin(string ipPort) {
    join_s2s = (s2s_join*) gen_request;
    cout << makeIP_Port_Str(myAddress) << " <- " << ipPort << "  S2S Join " << join_s2s->req_channel << endl;
    if (thisServerChannels.count(join_s2s->req_channel) == 0) { //not subscribed on the server level
        thisServerChannels.insert(join_s2s->req_channel);
        channelSubbedServers[join_s2s->req_channel][ipPort] = true;
        join_s2s->req_type = S2S_JOIN;

        for (auto it = adjServers.begin(); it != adjServers.end(); it++){
            if (ipPort != it->first) { // dont send it back to where it came from 
                sendto(serverSocket, join_s2s, sizeof(s2s_join), 0, (sockaddr*)&(it->second), sizeof(sockaddr_in));
                channelSubbedServers[join_s2s->req_channel][makeIP_Port_Str(it->second)] = true;
                cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(it->second) << "  S2S Join " << join_s2s->req_channel << endl;
            }
        }
    } else {
        channelSubbedServers[join_s2s->req_channel][ipPort] = true;
        // cout << makeIP_Port_Str(myAddress) << "  : already subbed to " << join_s2s->req_channel << endl;
    }
}
void handleS2SLeave(string ipPort) {
    leave_s2s = (s2s_leave*)gen_request;
    cout << makeIP_Port_Str(myAddress) << " <- " << ipPort << "  S2S Leave " << leave_s2s->req_channel << endl;
    channelSubbedServers[leave_s2s->req_channel].erase(ipPort);
}

void handleS2SSay(sockaddr_in src_address) {
    say_s2s = (s2s_say*) gen_request;
    cout << makeIP_Port_Str(myAddress) << " <- " << makeIP_Port_Str(src_address) << "  S2S [" << say_s2s->req_channel << "]" << "[" << say_s2s->req_username << "] " << say_s2s->txt_field << endl;
    if (recentlySeenMsgIDs.count(say_s2s->unique_id) == 1) { // if unique_id in recentlySeenMsgIDs then send /leave to sever it came from to break loop
        leave_s2s->req_type = S2S_LEAVE;
        strcpy(leave_s2s->req_channel, say_s2s->req_channel);
        sendto(serverSocket, leave_s2s, sizeof(s2s_leave), 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
        channelSubbedServers[leave_s2s->req_channel].erase(makeIP_Port_Str(src_address));       
        cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(src_address) << "  S2S Leave " << say_s2s->req_channel << ", loop detected!" << endl;
    } else { 
        recentlySeenMsgIDs.insert(say_s2s->unique_id);
        say_s2s->req_type = S2S_SAY;

        // if any adjServers subscribed to chennel then send it to them 
        map<string, bool> subscribedServers = channelSubbedServers[say_s2s->req_channel]; 
        for (auto it = subscribedServers.begin(); it != subscribedServers.end(); it++){
            if (makeIP_Port_Str(src_address) != it->first) { // dont send it back to where it came from 
                sendto(serverSocket, say_s2s, sizeof(s2s_say), 0, (sockaddr*)&(adjServers[it->first]), sizeof(sockaddr_in));
                cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(adjServers[it->first]) << "  S2S [" << say_s2s->req_channel << "]" << "[" << say_s2s->req_username << "] " << say_s2s->txt_field << endl;
            }
        }

        // if any clients subscribed to chennel then send say_text to them, 
        if (channelUserIpPorts.count(say_s2s->req_channel) != 0) { 
            cout << makeIP_Port_Str(myAddress) << "  : sending to clients " << endl;
            strcpy(say_text->txt_channel, say_s2s->req_channel);
            strcpy(say_text->txt_username, say_s2s->req_username);
            strcpy(say_text->txt_text, say_s2s->txt_field);

            // for each user subscribed to say_s2s->req_channel send say_text to. 
            set<string> channelUsers = channelUserIpPorts[say_s2s->req_channel];
            for (auto it = channelUsers.begin(); it != channelUsers.end(); it++) {
                    sendto(serverSocket, say_text, sizeof(text_say), 0, (sockaddr*)&(userIpPorts[*it].second), sizeof(sockaddr_in));
            }
        } else { // else send an s2s_leave to them but only if you are a leaf server in the channel topology
            // cout << makeIP_Port_Str(myAddress) << "  : sever has no clients on " << say_s2s->req_channel << endl; 
            if (channelSubbedServers[say_s2s->req_channel].size() < 2) { 
                leave_s2s->req_type = S2S_LEAVE;
                strcpy(leave_s2s->req_channel, say_s2s->req_channel);
                sendto(serverSocket, leave_s2s, sizeof(s2s_leave), 0, (sockaddr*)&src_address, sizeof(sockaddr_in));
                cout << makeIP_Port_Str(myAddress) << " -> " << makeIP_Port_Str(src_address) << "  S2S Leave " << say_s2s->req_channel << endl;
                thisServerChannels.erase(say_s2s->req_channel);
            } 
        }
    }
}
