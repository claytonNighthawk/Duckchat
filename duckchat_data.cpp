#include <netinet/in.h>
#include <utility>
#include <map>
#include <set>
#include <string>

class duckchat_data {
protected:
    map<string, pair<string, sockaddr_in>> userIpPorts; 
    map<string, set<string>> channelUserIpPorts; 

    set<unsigned int> recentlySeenMsgIDs;
    map<string, sockaddr_in> adjServers; 
    set<string> thisServerChannels;


public:
    duckchat_data();
    ~duckchat_data();
    
};