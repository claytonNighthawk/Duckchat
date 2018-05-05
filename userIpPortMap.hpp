#include <netinet/in.h>
#include <utility>
#include <map>
#include <mutex>
#include <iterator>

class userIpPortMap : public std::iterator {
protected:
    // map of user ip:ports to a pair of user names and sockaddr_in structs 
    std::map<std::string, std::pair<std::string, sockaddr_in>> userIpPorts; 
    std::mutex lock_;

public:
    userIpPortMap();
    ~userIpPortMap();
    
};