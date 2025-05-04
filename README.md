# Client-Server-Chat-Application
It is a client server chat application.
Socket Programming Client-Server Chat Application 
Aim: Socket Programming Chat Application (Windows - C with Winsock) 
Description: 
This project is a multi-client chat application implemented using C and Winsock2 on Windows. It 
supports: - Multiple client connections - Group messaging - Private messages using @username syntax - User authentication (username & password) - Chat logging (chatlog.txt) on server side 
Tools & Technology Used: - Language: C - Platform: Windows - Library: Winsock2 - IDE: VS Code - Compiler: GCC (MinGW) 
How to Compile & Run: 
1. Install MinGW (if not installed) 
Make sure gcc is accessible via your PATH.  
To check: 
gcc --version 
2. Open Terminal in Project Folder 
Open Command Prompt in the folder where server.c and client.c are located. 
3. Compile Server 
gcc -o chat_server.exe chat_server.c -lws2_32 
4. Compile Client 
gcc -o chat_client.exe chat_client.c -lws2_32 
5.  Run the Server 
chat_server.exe 
6.  Run One or More Clients 
chat_client.exe 
Example Chat Flow: - Client enters username & password - Server authenticates user - Clients can: - Send public messages: Hello everyone! - Send private messages: @yug Hello Yug 
Log File: 
All messages are saved to: chatlog.txt in the server's directory with timestamps. 


