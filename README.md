# PatChat
Simple Chatting Application, with a server written in C, and Client in C++ and C#

##Features
1. Heartbeat from server to Clients connected every 5-ish seconds
2. Broadcast a message to all connected clients
3. Send a message to a specific client
4. Create username
5. Login as a user
6. List all connected users
7. Endless unique usernames of up to ten characters
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

**BEAT**

HEart beat sent from server to clients

**ACK**

Client acknowledge heartbeat

##Challenges
- Client recieve. The client must listen all the time for an incoming message from the server. To overcome this a thread is created which listens all the time, by using an Asynchronous `NetworkStream.BeginRead()` in a while loop. 
- Server HeartBeat. The server sends a heart beat to the connected clients every 5 seconds - this is handled in it's own thread.
- Server Username storage. The server saves new usernames to a text file, and reads from this file on startup. This gives persistent usernames so a if the server is quit the usernames are remembered.

##To Improve
- Dynamic commands, maybe from a setup file etc
- Server
  - [X] Connected client FD in a struct with their username too.
  - [X] Thread ID's stored in a separate array - should be in the `CURRENT_CONNECTIONS` array or structs
  - [ ] The server doesn't act on the heartbeat ACK from the clients, either remove it or find a use for it
  - [ ] The server had no way of being quit once it's running, so although the code in place to free memory used and join the threads, it's never used
  - [ ] Refactor the source code into different files
  - [ ] Dynamic list of usernames which can grow/shink as clients connect
  - [X] Saving usernames to a file for later re-use
  - [ ] Options Flags
- Client
  - [ ] ~~If running for ages the garbage collector throws an error as it's unable to allocated some memory - not sure yet where it's being allocated.~~ Doesn't seem to be present in QT Gui
  - [X] GUI would be nice, failing that a more smart console interface
  - [ ] LOGOUT command
  - [ ] ~~Some client side checking of the replies from the Server or the input commands~~ Server side does the checking so this is pointless extra processing


##Server Design
Although not a truly stateful server the diagram below aids its design
<img src ="ChatServer/ServerStates.png">

##GUI Images
<img src ="QTChatClient/Images/onOpen.png" width = 150>
<img src ="QTChatClient/Images/onConnect.png" width = 150>
<img src ="QTChatClient/Images/onOptions.png" width = 150>
<img src ="QTChatClient/Images/onInitialMsg.png" width = 150>
<img src ="QTChatClient/Images/onUse.png" width = 150>
