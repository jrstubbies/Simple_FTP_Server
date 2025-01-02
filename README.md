# Simple_FTP_Server

A C++ program that uses sockets to connect a server and client acting as a basic File Transfer Protocol. 
This server is cross-platform compatible and works for both IPv4 and IPv6 connections as well. The sever
is capable of handling up to 5 simultaneous connections (though this can be made more or less by changing the
'5' on line 177). To access the servers files the client will need to provide a valid login, in this case
this has been hardcoded to simply be:
    Username = user
    Password = 123

By deafult the server will run on localhost and port 1234. However, when executing 'server.exe' can input
an argument. This will change the port the server will listen on. 


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  NOTE:                                                                                                      //
//     Startup code was provided by Massey teacher Napoleon Reyes to get cross platform functionality           //
//     (majority of the code blocks with "#if defined"), along with code for 'quit' 'syst' and 'port' commands. //
//                                                                                                              //
//     My contributions are converting IPv4 only data structs to IPv4/IPv6 data structs, 'user' 'pass' 'type'   //
//     'eptr' and 'list' commands, as well as socket set up.                                                    //
//                                                                                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


How to use:
    Server:
        - Build the server.cpp file using the makefile (cross platform ready)
        - Run the server.

    Client:
        - create a folder for the client 
        - open cmd prompt and change to this directory
        - To establish connection to FTP server,   type:
            - ftp
            - debug
            - open localhost 1234
        - Enter the username and password (in this case "user" and "123" respectively)
        - can now start using the commands.


    Client commands available (if logged in):
        - "binary" or "ascii"
            - This will change the file type to either binary (for non-text files) and ascii (text based files)

        - "get 'filename.type'"
            - get followed by a file name will transfer that file from the server to the client folder
        
        - "dir"
            - On the client side this will print the server's available files. On the server side this will create a 
            "tmp.txt" file with this same information.

        - "bye" or "disconnect"
            - this will terminate the CLIENT'S connection to the server.