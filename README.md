This project implements a TCP client and a server to manage subscriptions between clients and their preferred topics.

To send messages over TCP, knowing that a packet's maximum size is around 1600 bytes, I firstly send 2 bytes which represent
the size of the packet, sending the quotient and the remainder of the size divided by 256. After that, I send the packet.

When receiving, first I receive 2 bytes with which I calculate the size of the packet, then I receive bytes in a loop until receiving the whole packet.

Both the server and the clients use multiplexing to be able to read from different sockets.

The server has two vectors, one for subscribers and one for topics. A subscriber contains a file descriptor, an id which is unique,
a vector which contains topics to which he subscribed and a vector for topics which he missed while he was offline. A topic is defined
by its name, a field "sf" which marks it if it requires store and forward, the ocntent and its length. Store and forward is a functionality
which allows subscribers to mark a topic as such and receive every message that was sent while they were offline.
