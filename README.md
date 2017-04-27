An overview of the router program and its functionality.

Programming Assignment 2
Linked State Routing

Introduction
============
The router program will run on multiple computers in order to accurately simulate a network of packet-switching programs, much like IP routers that forwards datagrams over the Internet. The software will determine the shortest-path routes through a network where routers may crash in order to send messages along these routers using link-state protocol and Dijkstra's algorithm for the calculation of the shortest path.

Interface
=========
To start the router on an individual computer, execute the following commands:

--- Build ----
$ make all

--- Run ----
$ make N

where N is the nth router to be initialized. To add a router to an existing network, edit the Makefile and a line in the following format (be sure to include the tab):
N:
	./router id port host1 port1 host2 port2 ...

The id is a number between 0 and 19 inclusive that should be unique over all programs in a particular simulated network. The port is the number of a UDP port which the router will send and receive packets. The hostN and portN pairs are host names and ports respectively and are the declared neighbors of the this router.

For simplicity, the network described in the Makefile consist only of five routers where each router has at least one neighbor. To explicitly stop any of the running router, input the Ctrl+C command on the terminal. If the network has N routers then this process has to be done N times, otherwise the router will continue to run indefinitely as it was designed to do. The structure of the network can be changed, but remember that all routers have bi-directional links, thus if router A has a link to B then B also have a link to A.

Abstract
========
The router program is a simple implementation of linked-state routing using datagrams with each router in the network maintaining a routing table of the costs to all other routers. The router will periodically send linked-state packet (LSP) to other routers in an attempt to verify that the connections the router has with its neighbors which are given as arguments when the router was initialized are still active. If a connection is inactive then one of its neighbors is most likely down and the router will update its routing table accordingly, in order to reflect the most recent state of the network. By doing so, the router will know ahead of time whether it is possible to send a packet to its neighbor routers and if it is not, then the router will immediately know to discard the packet since the destination is unreachable.

There are two ways of running this router. The first possibility is to treat each router as a process and run multiple routers on the same computer, thus creating one of the most ideal network where the possibility of a down router and traffic is negligible. The second possibility is to run a copy of the program on multiple computers where the number of computers should be not greater than twenty. Each computer will represent a router ready to be initialized with a unique make instruction, the router will start pinging its neighbors and flooding the network with its routing table to get a sense of the structure of the network before sending packets to a specified destination.

Initialization of Routers
=========================
The make instruction which is unique for each router in the network is actually calling the router and passing the information about the router and its neighbors. The number of arguments (argc) should be at least two which actually initializes an isolated router and every subsequent argument must increase the argument count by two (to include host and port), thus the argument number will always be odd if the router is to be initialized properly.

After validating the correctness of the arguments, the router will create a Router object using a struct which contains the id, port number, link or neighbor count, a linked-state packet, array of the links, and a routing table. Then the router will iterate through the arguments and add a new entry to the array of links which contain the this router and its port number, its neighbor and its port number, and the cost which is set to infinity since the router has not ping its neighbors.

The router will then create and bind a socket to the specified port number and initialize the routing table. Four pthreads will be created and will perform the following operations: waiting for and processing incoming packets, periodically ping its neighbors, periodically send its routing table to its neighbors, and waiting for and accepting requests from the user to send a packet to a specified router in the network. By using pthreads, this allows the router to handle multiple tasks at once and does not require that all routers need to be synchronized. For example, if a new router joins the network and starts pinging even if its neighbors are busy, they will still be able to ping back.

The Cycle & Attributes of a Router
==========================
At the beginning of the router's lifecycle, threads previously initialized have to wait a certain amount of seconds before starting its operation which in this case is either pinging or flooding since the other threads will only operate if there is an event. The time spent waiting is specified by their respective interval variable. The ping interval, set to 1 seconds, will always be less than the flood interval, set to 10 seconds, since the router needs to update its initially empty routing table before flooding the network. After the set amount of time has passed, the ping and flooding thread will start sending packets to the router's neighbors.

The ping thread sends a LSP to all the router's neighbors along with the send time in order to allow the receiver to calculate the transmission time of the packet which will later be used as the cost between the send and the receiver in the network. After pinging all of its neighbors, the thread will sleep for the specified interval and repeat this process.

The flooding thread will send LSP to all the router's neighbors n times where n is the number of nodes currently active in the network since it takes at most n steps to send a packet from the starting router to another router that it is indirectly connected to but is the farthest away. By doing so, every router in the network will have the same copy of the routing table. For example, in a network of three routers where all routers are connected to each other then the exchange of routing table for all routers to have an exact copy is only one step. However, if router A is connected to B and B is connected to C then it takes flooding at least two steps for the routing table of A to get to C. Similar to the ping thread, flooding waits for a specified amount of time before continuing to flood the network again.

The messaging thread (msg) starts when an event occurs which in this case is when the user types the request "msg" in the terminal. The program will immediately ask where to send the message to. If the destination specified is unreachable which can be easily done using a linear search in the routing table and checking if the cost to the destination is infinity. If so, then the program will complain to the user. Otherwise, the program will calculate the next hop to the destination and send the message to the next router which will in turn pass the message until it finally reaches its destination. The user will only be allowed to send message every ten seconds and is not required to send one after each interval since message are meant to be send from time to time.

The incoming thread is also event-driven since it requires a packet to be sent to the router before it can start any operation. Depending on the header of the packet, the thread will handle each individual packet differently. Note that all the routers in the network can only send a LSP and attached to each is a header which contains a variable specifying the type of the packet. There are three types of LSP-format packet and they are the following: ping, routable (routing table), and message. 

If the packet is of type ping, then the program will send a ping back to the sender to acknowledge its connection, and save the sender ID into an array to keep track of the neighbors that are alive at this moment in time. In addition, the packet will contain the send time which the router will use to calculate the transmission time and set it as the cost between it and the sender in the routing table.

For the routable type packet, the router will update its routing table with the routing table it has received from the sender. When the number of routing tables received from all senders is equal to the number of nodes in the network times the number of neighbors the router has, then convert the routing table into an adjacency matrix. During the conversion, the router will iterate through the array of all the routers that have ping this router and if a router ID is in the routing table but not in the array then it is most likely the router missing is down at the moment and should not be included in the matrix. Once the matrix has been created, it will be passed to the Dijkstra algorithm function which will calculate the shortest path from the router to every other routers in the network. This information will be summarized in a table which will be displayed on the terminal.

Finally for the message type packet, the router checks if the packet destination ID is equal to the router ID and if so then the message arrived to its destination. Otherwise, the router will calculate the next hop and send the packet to the next router.
