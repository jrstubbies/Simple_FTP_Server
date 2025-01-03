//=======================================================================================================================
// Course: 159.342
// Description: Cross-platform, Active mode FTP SERVER, Start-up Code for Assignment 1 
//
// Start Up Code Author: n.h.reyes@massey.ac.nz
//=======================================================================================================================


// Set the header files depending on the OS type
#if defined __unix__ || defined __APPLE__
    #include <unistd.h>
    #include <errno.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>              //used by getnameinfo()
    #include <iostream>

#elif defined __WIN32__
    #include <winsock2.h>
    #include <ws2tcpip.h>           //required by getaddrinfo() and special constants
    #include <stdlib.h>
    #include <stdio.h>
    #include <iostream>
    #define WSVERS MAKEWORD(2,2)    // Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h. Set to version 2.2
    WSADATA wsadata;                //Create a WSADATA object called wsadata. 
#endif


#define USE_IPV6 true               // If false IPv4 addressing will be used; you need to set this to true to enable IPv6 later on
#define BUFFER_SIZE 256
#define DEFAULT_PORT "1234"

enum class FileType { BINARY, TEXT, UNKNOWN };
FileType file_type;

//*******************************************************************
//                                MAIN
//*******************************************************************
int main(int argc, char* argv[]) 
{
    printf("\n============================================\n");
    printf("   << Cross-platform FTP Server >>\n");
    printf("============================================\n");
    printf("   Myles Stubbs    \n");
    printf("============================================\n");


    //********************************************************************
    //                      INITIAL CHECK
    //********************************************************************
    
    // check that the socket api is available, then check that version number of the socket is 2.2
    #if defined __unix__ || defined __APPLE__
    #elif defined _WIN32
        int err = WSAStartup(WSVERS, &wsadata);
        if (err != 0) {
            WSACleanup();
            printf("WSAStartup failed with error: %d\n", err);
            exit(1);
        }

        if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2) {
            printf("Could not find a usable version of Winsock.dll\n");
            WSACleanup();
            exit(1);
        }
    #endif	
    

    //********************************************************************
    //                              SOCKET SETUP
    //********************************************************************
    
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    int iResult, addrlen, serverPort;

    // Set the family type to either IPv6 or IPv4
    memset(&hints, 0, sizeof(struct addrinfo));
    if (USE_IPV6) {
        hints.ai_family = AF_INET6;
    } else { 
        hints.ai_family = AF_INET;
    }
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    
    hints.ai_flags = AI_PASSIVE;       


    // converts human-readable hostname or IP address into binary representation
    if (argc == 2) {
        iResult = getaddrinfo(NULL, argv[1], &hints, &result);
        serverPort = atoi(argv[1]);  
    } else {
        iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
        serverPort = atoi(DEFAULT_PORT); 
    }
    
    // If any errors in setting iResult then exit, and cleanup if on windows
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
    #if defined _WIN32
            WSACleanup();
    #endif    
            return 1;
        }


    // Declare sockets for different OS 
    #if defined __unix__ || defined __APPLE__
        int s, ns;                
        int s_data_act = -1; 
    #elif defined _WIN32			 
        SOCKET s, ns;                
        SOCKET s_data_act = INVALID_SOCKET;                     
    #endif


    // set the server's listening socket, if there is any issues then exit, cleanup for windows
    s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    #if defined __unix__ || defined __APPLE__
        if (s < 0) {
            printf("socket failed\n");
            freeaddrinfo(result);
            exit(1);
        }
    #elif defined _WIN32
        if (s == INVALID_SOCKET) {
            printf("Error at socket(): %d\n", WSAGetLastError());
            freeaddrinfo(result);
            WSACleanup();
            exit(1);
        }
    #endif


    //********************************************************************
    //                          BIND THE SOCKET 
    //********************************************************************

    // bind the server's socket to the relevant IP address and port number
    iResult = bind(s, result->ai_addr, (int)result->ai_addrlen);

    //if error is detected, then clean-up
    #if defined __unix__ || defined __APPLE__
        if (iResult == -1) {
            printf("\nbind failed\n");
            freeaddrinfo(result);
            close(s);//close socket
    #elif defined _WIN32
        if (iResult == SOCKET_ERROR) {
            printf("bind failed with error: %d\n", WSAGetLastError());
            freeaddrinfo(result);
            closesocket(s);
            WSACleanup();
    #endif       
            exit(1);
        }
    
        freeaddrinfo(result); 



    //********************************************************************
    //                      SET SOCKET TO LISTEN
    //********************************************************************

    // Set the socket to listen, and set the queue request length (up to 5 for windows)
    #if defined __unix__ || defined __APPLE__
        if (listen(s, SOMAXCONN) == -1) {
    #elif defined _WIN32
        if (listen(s, 5) == SOCKET_ERROR) {
    #endif

    #if defined __unix__ || defined __APPLE__
            printf("\nListen failed\n");
            close(s);
    #elif defined _WIN32
            printf("Listen failed with error: %d\n", WSAGetLastError());
            closesocket(s);
            WSACleanup();
    #endif   
            exit(1);
        }


    //********************************************************************
    //                  VARIABLES FOR MAIN LOOP
    //********************************************************************
    int count = 0, bytes;                                          
    char send_buffer[BUFFER_SIZE], receive_buffer[BUFFER_SIZE];     // arrays to hold sent/received message
    memset(&send_buffer, 0, BUFFER_SIZE);       
    memset(&receive_buffer, 0, BUFFER_SIZE);
    
    struct sockaddr_storage clientAddress;              
    struct sockaddr_storage local_data_addr_act;        
    char clientHost[NI_MAXHOST];         // client's IP address
    char clientService[NI_MAXSERV];      // client's Port number
    char username[50], password[50];
    int active = 0;                      // marks active data connection (0 = no, 1 = yes)
    int authorised = 0;                  // check valid login (0 = no, 1 = yes)

    
    //********************************************************************
    //                          INFINITE MAIN LOOP START
    //********************************************************************
    while (1) {
        printf("\n------------------------------------------------------------------------\n");
        printf("SERVER is waiting for an incoming connection request at port:%d\n", serverPort);      
        printf("------------------------------------------------------------------------\n");

        
        //********************************************************************
        //      NEW SOCKET FOR CURRENT CLIENT'S CONTROL CONNECTION
        //********************************************************************
        addrlen = sizeof(clientAddress);
        
        #if defined __unix__ || defined __APPLE__     
                ns = accept(s, (struct sockaddr*)(&clientAddress), (socklen_t*)&addrlen);
        #elif defined _WIN32      
                // store the binary representation of the clients address and port number in 'clientAddress'
                ns = accept(s, (struct sockaddr*)(&clientAddress), &addrlen);           
        #endif

        // Check if the new socket is created successfully, if it is then get the Clients IP and port info
        #if defined __unix__ || defined __APPLE__
                if (ns == -1) {
                    printf("\naccept failed\n");
                    close(s);
                    return 1;
                }
                else {
                    int returnValue;
                    memset(clientHost, 0, sizeof(clientHost));
                    memset(clientService, 0, sizeof(clientService));

                    returnValue = getnameinfo((struct sockaddr*)&clientAddress, addrlen, clientHost, sizeof(clientHost),
                        clientService, sizeof(clientService), NI_NUMERICHOST);
                    if (returnValue != 0) {
                        printf("\nError detected: getnameinfo() failed \n");
                        exit(1);
                    }
                    else {
                        printf("\nConnected to [CLIENT's IP %s, Port:%s] through SERVER's port %d\n", clientHost, clientService, serverPort);
                    }
                }

        #elif defined _WIN32
                if (ns == INVALID_SOCKET) {
                    printf("accept failed: %d\n", WSAGetLastError());
                    closesocket(s);
                    WSACleanup();
                    return 1;
                }
                else {
                    DWORD returnValue;
                    memset(clientHost, 0, sizeof(clientHost));
                    memset(clientService, 0, sizeof(clientService));

                    returnValue = getnameinfo((struct sockaddr*)&clientAddress, addrlen, clientHost, sizeof(clientHost),
                        clientService, sizeof(clientService), NI_NUMERICHOST);
                    if (returnValue != 0) {
                        printf("\nError detected: getnameinfo() failed with error#%d\n", WSAGetLastError());
                        exit(1);
                    }
                    else {
                        printf("\nConnected to [CLIENT's IP %s, Port:%s] through SERVER's port %d\n", clientHost, clientService, serverPort);
                            
                    }
                }
        #endif 

        //********************************************************************
        //                    Respond with welcome message            
        //*******************************************************************
        count = snprintf(send_buffer, BUFFER_SIZE, "220 FTP Server ready. \r\n");
        if (count >= 0 && count < BUFFER_SIZE) {
            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        }


        //********************************************************************
        //                  COMMUNICATION LOOP (per client)
        //********************************************************************
        file_type = FileType::UNKNOWN;
        int n;
        while(1) {
            
            //********************************************************************
            //              RECEIVE MESSAGE AND THEN FILTER IT
            //********************************************************************

            n = 0;
            while(1) {  
                // receive the message byte-by-byte, removing any delimiting characters
                bytes = recv(ns, &receive_buffer[n], 1, 0);
                if((bytes <= 0) || (bytes == SOCKET_ERROR)) break;
                if(receive_buffer[n] == '\n') {
                    receive_buffer[n] = '\0';
                    break;
                }
                if(receive_buffer[n] != '\r') n++;
            }

            if(bytes == 0) {
                printf("\nThe client has gracefully exited the connection.\n");
                break;
            }
            if ((bytes < 0) || (bytes == 0)) break;
            

            //********************************************************************
            //                  PROCESS COMMANDS/REQUEST FROM USER
            //********************************************************************	
            printf("[DEBUG INFO] command received:  '%s\\r\\n' \n", receive_buffer);
            
            // Username input   (USER)
            if (strncmp(receive_buffer,"USER",4)==0) {
                printf("Logging in... \n");

                sscanf(receive_buffer + 5, "%s", username);     // store what the user types for the username

                count=snprintf(send_buffer,BUFFER_SIZE,"331 Password required \r\n");
                if(count >=0 && count < BUFFER_SIZE){
                    bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                }
                printf("[DEBUG INFO] <-- %s\n", send_buffer);           
                if (bytes < 0) break;
            }

            // Password input   (PASS)
            if (strncmp(receive_buffer,"PASS",4)==0)  {
                sscanf(receive_buffer + 5, "%s", password);          // Extract password
                
                // Check if user is valid/authenticated
                if(strcmp(username, "user") == 0 && strcmp(password, "123") == 0) {
                    count=snprintf(send_buffer,BUFFER_SIZE,"230 User logged in \r\n");
                    authorised = 1;
                } else {
                    count = snprintf(send_buffer, BUFFER_SIZE, "530 Not logged in\n");
                    authorised = 0;
                }
            
                if(count >=0 && count < BUFFER_SIZE){
                    bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                }
                printf("[DEBUG INFO] <-- %s\n", send_buffer);    
                if (bytes < 0) break; 
            }
            
            // OPTS
            if (strncmp(receive_buffer,"OPTS",4)==0)  
            {
                printf("unrecognised command \n");
                count=snprintf(send_buffer,BUFFER_SIZE,"502 command not implemented\r\n");					 
                if(count >=0 && count < BUFFER_SIZE){
                    bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                }
                printf("[DEBUG INFO] <-- %s\n", send_buffer);          
                if (bytes < 0) break;
            }            
            
                
            // Users close the connection       (QUIT)
            if (strncmp(receive_buffer,"QUIT",4)==0)  
            {
                printf("Exiting control connection... \n");

                count=snprintf(send_buffer,BUFFER_SIZE,"221 Control connection closed by client\r\n");					 
                if(count >=0 && count < BUFFER_SIZE){
                    bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                }
                printf("[DEBUG INFO] <-- %s\n", send_buffer);
                if (bytes < 0) break;
                
                // close sockets and reset 'active' and 'authorised' variables
                if(active == 1){
                    #if defined __unix__ || defined __APPLE__ 
                        close(s_data_act);
                    #elif defined _WIN32					 
                        closesocket(s_data_act);        
                    #endif
                }
                #if defined __unix__ || defined __APPLE__ 
                    close(ns);
                #elif defined _WIN32					 
                    closesocket(ns);             
                #endif

                active = 0;                                         
                authorised = 0;
                break;      
            }



            // These server commands can only be accessed if the user has valid login
            if (authorised) {
                
                // Information on the OS    (SYST)
                if (strncmp(receive_buffer,"SYST",4)==0)  
                {
                    printf("Information about the system \n");
                    count=snprintf(send_buffer,BUFFER_SIZE,"215 Windows 64-bit\r\n");					 
                    if(count >=0 && count < BUFFER_SIZE){
                        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                    }
                    printf("[DEBUG INFO] <-- %s\n", send_buffer);
                    if (bytes < 0) break;
                }


                // Change the file type modifier        (TYPE)
                if (strncmp(receive_buffer,"TYPE",4)==0) {
                    printf("<--TYPE command received.\n\n");

                    bytes = 0;
                    char objType;
                    int scannedItems = sscanf(receive_buffer, "TYPE %c", &objType);

                    // check for correct command syntax
                    if (scannedItems < 1) {
                        count = snprintf(send_buffer, BUFFER_SIZE, "501 syntax error in argument\n");
                        if (count >= 0 && count < BUFFER_SIZE) {
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);           
                        if (bytes < 0) break; 
                    }

                    // Update the 'FileType' based on 'binary' or 'ascii' input
                    switch(toupper(objType)) {
                        case 'I':
                            file_type = FileType::BINARY;
                            printf("Using binary mode to transfer files.\n");
                            count = snprintf(send_buffer,BUFFER_SIZE,"200 Type set to I.\r\n");
                            if (count >= 0 && count < BUFFER_SIZE) {
                                bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                            }
                            printf("[DEBUG INFO] <-- %s\n", send_buffer);           
                            if (bytes < 0) break; 
                            break;                      // break switch
                        
                        case 'A':
                            file_type = FileType::TEXT;
                            printf("Using ASCII mode to transfer files.\n");
                            count = snprintf(send_buffer,BUFFER_SIZE,"200 Type set to A.\r\n");
                            if (count >= 0 && count < BUFFER_SIZE) {
                                bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                            }
                            printf("[DEBUG INFO] <-- %s\n", send_buffer);           
                            if (bytes < 0) break; 
                            break;

                        default:
                            count = snprintf(send_buffer,BUFFER_SIZE,"501 Syntax error in argument.\r\n");
                            if (count >= 0 && count < BUFFER_SIZE) {
                                bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                            }
                            printf("[DEBUG INFO] <-- %s\n", send_buffer);           
                            if (bytes < 0) break; 
                            break;
                    } // end of switch
                }


                // Server stores a file from client     (STOR)
                if (strncmp(receive_buffer,"STOR",4)==0)  
                {
                    printf("unrecognised command \n");
                    count=snprintf(send_buffer,BUFFER_SIZE,"502 command not implemented\r\n");					 
                    if(count >=0 && count < BUFFER_SIZE){
                        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                    }
                    printf("[DEBUG INFO] <-- %s\n", send_buffer);
                    if (bytes < 0) break;
                }


                // Retrieve a file from the server       (RETR)
                if(strncmp(receive_buffer, "RETR", 4) ==0){
                    char filename[BUFFER_SIZE];     
                    memset(&filename, 0, BUFFER_SIZE);      

                    if (sscanf(receive_buffer, "RETR %s", filename) != 1) {
                        count = snprintf(send_buffer, BUFFER_SIZE, "501 Syntax error in command argument\n");                     
                        if (count >= 0 && count < BUFFER_SIZE) {
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }  
                        if (bytes < 0) break;
                        continue;
                    }

                    // Check if the file exists
                    FILE *fin = fopen(filename, "r");
                    if (fin == NULL) {
                        count = snprintf(send_buffer, BUFFER_SIZE, "550 File not found; Closing data connection\n");                     
                        if (count >= 0 && count < BUFFER_SIZE) {
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        if (bytes < 0) break;
                    } else {
                        // File exists, proceed with transferring
                        count = snprintf(send_buffer, BUFFER_SIZE, "150 Data connection opened; Transferring file.\r\n");
                        if (count >= 0 && count < BUFFER_SIZE) {					  
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        if (bytes < 0) break;

                        char file_buffer[BUFFER_SIZE];
                        memset(file_buffer, 0, BUFFER_SIZE);
                        
                        // Send file based on the type they have selected
                        if (file_type == FileType::BINARY) {
                            fin = fopen(filename, "rb");
                            while ((bytes = fread(file_buffer, 1, sizeof(file_buffer), fin)) > 0) {
                                send(s_data_act, file_buffer, bytes, 0); 
                            }
                        } else if (file_type == FileType::TEXT) {
                            fin = fopen(filename, "r");
                            while (fgets(file_buffer, sizeof(file_buffer), fin) != nullptr) {
                                send(s_data_act, file_buffer, strlen(file_buffer), 0);
                            }
                        }

                        count = snprintf(send_buffer, BUFFER_SIZE, "226 File transfer completed; Closing data connection.\r\n");
                        if (count >= 0 && count < BUFFER_SIZE) {
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        if (bytes < 0) break;
                    }
                    
                    // close the file and the data connection
                    fclose(fin); 
                    active = 0;   

                    // Close data connection socket
                    #if defined __unix__ || defined __APPLE__ 
                        close(s_data_act);
                    #elif defined _WIN32					 
                        closesocket(s_data_act);             
                    #endif 
                }


                // Changing the directory       (CWD)
                if (strncmp(receive_buffer,"CWD",3)==0)  
                {
                    printf("unrecognised command \n");
                    count=snprintf(send_buffer,BUFFER_SIZE,"502 command not implemented\r\n");					 
                    if(count >=0 && count < BUFFER_SIZE){
                        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                    }
                    printf("[DEBUG INFO] <-- %s\n", send_buffer);
                    if (bytes < 0) break;
                }


                // Set up the data connection for IPv6 addresses     (EPRT)
                if (strncmp(receive_buffer, "EPRT", 4) == 0) 
                {
                    // Extract the protocol, address, and port from the EPRT command
                    char protocol;
                    char address[NI_MAXHOST], port_str[6];
                    memset(&address, 0, NI_MAXHOST);

                    // %c gets a single char ('1' or '2'), %[^|] reads chars until reaches the |, %5s gets the port number
                    int scannedItems = sscanf(receive_buffer, "EPRT |%c|%[^|]|%5s|", &protocol, address, port_str);

                    if (scannedItems != 3) {
                        snprintf(send_buffer,BUFFER_SIZE,"501 Syntax error in arguments \r\n");						
                        send(ns, send_buffer, strlen(send_buffer), 0);
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        break;
                    } 

                    // use 'getaddrinfo' to get the client's address info
                    memset(&local_data_addr_act, 0, sizeof(local_data_addr_act));            
                    struct addrinfo hints, *result =NULL;
                    memset(&hints, 0, sizeof(struct addrinfo));
                    
                    if(protocol == '2') {
                        hints.ai_family = AF_INET6;
                    } else {
                        hints.ai_family = AF_INET;
                    }
                    hints.ai_socktype = SOCK_STREAM;
                    
                    if (getaddrinfo(address, port_str, &hints, &result) != 0) {
                        printf("Error resolving address\n");
                        snprintf(send_buffer, BUFFER_SIZE, "501 Syntax error in address \r\n");
                        send(ns, send_buffer, strlen(send_buffer), 0);
                        break;
                    }

                    // Copy the first resolved address information to local_data_addr_act
                    memcpy(&local_data_addr_act, result->ai_addr, result->ai_addrlen);

                    // set the socket using 'results' from getaddrinfo
                    s_data_act = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
                    if (s_data_act == -1 || s_data_act == INVALID_SOCKET) {
                        printf("Error creating socket for data connection\n");
                        snprintf(send_buffer, BUFFER_SIZE, "425 Can't open data connection");
                        send(ns, send_buffer, strlen(send_buffer), 0);
                        closesocket(s_data_act);
                        freeaddrinfo(result);
                        break;
                    }

                    active = 1;               // acknowledge that an active connection has been made.
                    freeaddrinfo(result);     // free memory of result

                    printf("\tCLIENT's IP is %s\n",address);
                    printf("\tCLIENT's Port is %s\n",port_str);
                    printf("===================================================\n");
                }


                // PORT
                if(strncmp(receive_buffer,"PORT",4)==0) 
                {
                    s_data_act = socket(AF_INET, SOCK_STREAM, 0);

                    int act_port[2];
                    int act_ip[4], port_dec;
                    char ip_decimal[NI_MAXHOST];
                    printf("===================================================\n");
                    printf("\tActive FTP mode, the client is listening... \n");
                    active=1;           
                    
                    int scannedItems = sscanf(receive_buffer, "PORT %d,%d,%d,%d,%d,%d", &act_ip[0],&act_ip[1],&act_ip[2],&act_ip[3],
                                                        &act_port[0],&act_port[1]);
                    
                    if(scannedItems < 6) {
                        count=snprintf(send_buffer,BUFFER_SIZE,"501 Syntax error in arguments \r\n");						
                        if(count >=0 && count < BUFFER_SIZE){
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        if (bytes < 0) break;
                    }

                    memset(&local_data_addr_act, 0, sizeof(local_data_addr_act)); // Initialize to zero
                    
                    // Create a new IPv4 structre and assign it the memory address of local_data_addr_act
                    struct sockaddr_in *ipv4 = (struct sockaddr_in *)&local_data_addr_act;
                    ipv4->sin_family = AF_INET;
                    
                    // use pton to convert human-readable address into binary.
                    count=snprintf(ip_decimal,NI_MAXHOST, "%d.%d.%d.%d\n", act_ip[0], act_ip[1], act_ip[2],act_ip[3]);   
                    if(!(count >=0 && count < BUFFER_SIZE)) break;                
                    inet_pton(AF_INET, ip_decimal, &ipv4->sin_addr);
                    printf("\tCLIENT's IP is %s\n",ip_decimal);
                    
                    // use bitwise on n5 to get most significant bits, add 'n6' to get the client port number
                    port_dec=act_port[0];
                    port_dec=port_dec << 8;
                    port_dec=port_dec+act_port[1];
                    ipv4->sin_port = htons(port_dec);
                    printf("\tCLIENT's Port is %d\n",port_dec);
                    printf("===================================================\n");
                }


                // LIST (making LIST and NLIST have same functionality)
                if ((strncmp(receive_buffer,"LIST",4)==0) || (strncmp(receive_buffer,"NLST",4)==0))   
                {
                    #if defined __unix__ || defined __APPLE__ 
                        int i=system("ls -la > tmp.txt");
                    #elif defined _WIN32	
                        int i=system("dir > tmp.txt");
                    #endif
                    
                    // Inform user the data is about to be sent
                    count=snprintf(send_buffer,BUFFER_SIZE,"150 File now transferring.... \r\n");     
                    if(count >=0 && count < BUFFER_SIZE) {					  
                        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                    } 
                    printf("[DEBUG INFO] <-- %s\n", send_buffer); 
                    if(bytes < 0) break;

                    //open tmp.txt file and read the lines and send to the client
                    FILE *fin=fopen("tmp.txt","r");
                    char temp_buffer[80];
                    
                    while (!feof(fin)) {
                        fgets(temp_buffer,78,fin);
                        sprintf(send_buffer,"%s",temp_buffer);
                        send(s_data_act, send_buffer, strlen(send_buffer), 0);
                    }

                    // close the file and set the data connection to be closed
                    fclose(fin);
                    active = 0;
                    
                    count = snprintf(send_buffer, BUFFER_SIZE, "226 File transfer OK; Data connection closing. \r\n");           
                    if(count >= 0 && count < BUFFER_SIZE){
                        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                    }
                    printf("[DEBUG INFO] <-- %s\n", send_buffer);

                    #if defined __unix__ || defined __APPLE__ 
                        close(s_data_act);
                    #elif defined _WIN32					 
                        closesocket(s_data_act);              
                    #endif
                }   


                // Connect function moved to be after the PORT and EPRT blocks. Only run this if an active data connection is required
                if(active) 
                {
                    // attempt to establish a data connection to client socket
                    if (connect(s_data_act, (struct sockaddr *)&local_data_addr_act, sizeof(local_data_addr_act)) != 0)
                    {
                        char host[NI_MAXHOST], service[NI_MAXSERV];
                        int iResult;
                        printf("trying connection in \n");
                       
                        iResult = getnameinfo((struct sockaddr *)&local_data_addr_act, sizeof(local_data_addr_act), host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICHOST);
                        printf("%s %s\n", host, service);
                    
                        count=snprintf(send_buffer,BUFFER_SIZE, "425 Can't start active connection... \r\n");
                        if(count >=0 && count < BUFFER_SIZE){
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        if(bytes < 0) break;

                        #if defined __unix__ || defined __APPLE__ 
                            close(s_data_act);
                        #elif defined _WIN32	
                            closesocket(s_data_act);
                        #endif	 

                        active = 0;
                    }

                    // if the connection is successful
                    else 
                    {
                        if(local_data_addr_act.ss_family == AF_INET) {
                            count=snprintf(send_buffer,BUFFER_SIZE, "200 PORT Command successful\r\n");
                        } else {
                            count=snprintf(send_buffer,BUFFER_SIZE, "200 EPRT Command successful\r\n");
                        }
                        
                        if(count >=0 && count < BUFFER_SIZE){
                            bytes = send(ns, send_buffer, strlen(send_buffer), 0);  
                        }
                        printf("[DEBUG INFO] <-- %s\n", send_buffer);
                        if(bytes < 0) break;
                    }
                }   
            } 

            // if not logged in and trying to access comman that ISN'T USER, PASS, OPTS, QUIT, then tell to login
            if(!authorised && (strncmp(receive_buffer,"USER",4)!=0) && (strncmp(receive_buffer,"PASS",4)!=0) 
            && (strncmp(receive_buffer,"QUIT",4)!=0) && (strncmp(receive_buffer,"OPTS",4)!=0)) {
                snprintf(send_buffer, BUFFER_SIZE, "530 Need valid login to access server\n");
                send(ns, send_buffer, strlen(send_buffer), 0);
            }

        } // end of communication loop

        #if defined __unix__ || defined __APPLE__ 
			close(ns);
        #elif defined _WIN32	
			closesocket(ns);
        #endif

        printf("DISCONNECTED from %s\n", clientHost); 

    } // end of the main loop

    printf("\nSERVER IS SHUTTING DOWN....\n");
    #if defined __unix__ || defined __APPLE__ 
		 close(s);
    #elif defined _WIN32		 
        closesocket(s);
        WSACleanup();
    #endif		 
    
    return 0;
} // end of main
