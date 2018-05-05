#include <netinet/in.h>
#include <utility>
#include <map>
#include <set>
#include <string>

class duckchat_data {
protected:
    map<string, set<string>> channelUserIpPorts; // map of channel names to set of users (recorded by their ip:port numbers) subbed to that channel 

    set<unsigned int> recentlySeenMsgIDs;
    map<string, sockaddr_in> adjServers; // map of server ip:ports to sockaddr_in structs that represents the adjectent servers
    set<string> thisServerChannels; // all channels this server is subbed to because of clients or to pass to other servers.

    // map of channel names to a map of server ip:port strings to not in danger boolean for the soft join of servers on channels.
    // the keys of the outter map denote the servers that are subbed to that channel.
    // this will not include this server's subscribbed channels as that will be included in thisServerChannels.
    map<string, map<string, bool>> channelSubbedServers;


public:
    duckchat_data();
    ~duckchat_data();
    
};