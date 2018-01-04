# Duckchat
IRC-like chat program consisting of one or more servers and clients who can all connect over the internet using UDP sockets to communicate.  

Clients can connect to any of the servers in the topology and communicate with all clients subscribed to the same channel.

Servers use flood and prune protocol to detect and leave loops when sending messages.

Authors: Clayton Kilmer, Ashton Shears.
