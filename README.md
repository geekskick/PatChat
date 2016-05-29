# PatChat
Simple Chatting Application, with a server written in C, and Client in C#

##Features
1. Heartbeat from server to Clients connected every 5-ish seconds
2. Broadcast a message to all connected clients
3. Send a message to a specific client
4. Create username
5. Login as a user
6. List all connected users
7. Ten unique usernames
8. Up to 5 clients connected at once
9. Server ping

##Client Commands
**NEW USERNAME**

Usage: NEWUSER (name)

Creates a new username in the chat server

**BROADCAST MESSAGE**

Usage: BROADCAST (msg)

Sends a message to all connected users

**LIST USERS**

Usage: LIST 

Lists all corrently connected users

**PING**

Usage: PING

Get a PONG from the server

**SEND MESSAGE**

Usage: SEND $(user) (msg)

Send a message to the user specified, this is echo'd to the sender

##Challenges
- Client recieve. The client must listen all the time for an incoming message from the server. To overcome this a thread is created which listens all the time, by using an Asynchronous `NetworkStream.BeginRead()` in a while loop. 
- Server HeartBeat. The server sends a heart beat to the connected clients every 5 seconds - this is handled in it's own thread.

##To Improve
- Dynamic commands, maybe from a setup file etc
- Server
  * The connected clients' file descriptors are stored in an array, so are the current thread ids, however there is also a struct for each connection which contains the clients' file descriptors too. This struct should detail the connection's thread id, and that struct should be used rather than the arrays of client fd and thread ids
  - The server doesn't act on the heartbeat ACK from the clients, either remover it or find a use for it
  - The server had no way of being quit once it's running, so although the code in place to free memory used and join the threads, it's never used
  - Refactor the source code into different files
  - Dynamic list of usernames which can grow/shink as clients connect
  - Saving usernames to a file for later re-use
- Client
  - If running for ages the garbage collector throws an error as it's unable to allocated some memory - not sure yet where it's being allocated.
  - GUI would be nice, failing that a more smart console interface
  - Not all command functionality is in yet
  - Some client side checking of the replies from the Server or the input commands
