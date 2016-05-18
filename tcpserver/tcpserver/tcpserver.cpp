#include "stdafx.h"
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <ws2tcpip.h>



// Function declarations
void dealWithConnection(SOCKET * connection);
void incrementMsgSequence();


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "4660"

int msgSequence = 0;

void main(void) {
	int				result;
	
	WSADATA			wsaData;
	SOCKET			listeningSocket = INVALID_SOCKET;
	SOCKET			newConnection	= INVALID_SOCKET;
	SOCKADDR_IN		serverAddr;
	SOCKADDR_IN		clientAddr;
	int				port			= 4660;

	struct addrinfo *addrResult = NULL;
    struct addrinfo hints;

	// Initialize Winsock2
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (result != 0) {
        printf("WSAStartup failed with error: %d\n", result);
        return;
    }

	ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addrResult);
    if (result!= 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return;
    }

	// Create socket connection to listen for client connections
	listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listeningSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(addrResult);
        WSACleanup();
        return;
	}

	// We set up the server address to tell the listening socket how we want to listen for connections
	// from clients.
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// We link the address information to the listeningSocket, so it can listen correctly for connections
	result = bind(listeningSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
	if (result == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(addrResult);
        closesocket(listeningSocket);
        WSACleanup();
        return;
    }

	// Effectively listen for client connections, with a buffer of 5 max connections
	listen(listeningSocket, 5);
	printf("Listening for socket connections from any <address ip>, <port>: %d\n", port);
	for( ; ; ) {
		// Looks for new connection requests from client.
		newConnection = accept(listeningSocket, NULL, NULL);
		printf("Connection accepted\n");
		// On receival of connection, start a thread to deal with connection
		//_beginthread(dealWithConnection,0, &newConnection);
		dealWithConnection(&newConnection);
	
	}

	// Shutdown server socket
	result = shutdown(listeningSocket, SD_SEND);
    if (result == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        
		closesocket(listeningSocket);
        WSACleanup();
        return;
    }

    // Cleanup
    closesocket(listeningSocket);
    WSACleanup();
}

void dealWithConnection(SOCKET * connection) {
	printf("New connection opened\n");
	int result;
    char recvBuf[DEFAULT_BUFLEN];
    int recvBufLen = DEFAULT_BUFLEN;

	do {
		// Receives messages from clientConnections and returns error (-1) or bytes received
		result = recv(*connection, recvBuf, recvBufLen, 0);

		// Bytes were succesfully received
		if (result > 0) {
			printf("Bytes received: %d\n", result);
			printf("Message received: %s\n", recvBuf);
			printf("Size of message: %d\n", sizeof(recvBuf) / sizeof(recvBuf[0]));

			// Verify what type of message
			// AckMessage() - 17 bytes


		}
		
		// Connection close received
		else if (result == 0)
				printf("Connection closing...\n");
		
		// Error on receival
		else  {
			printf("recv failed with error: %d\n", WSAGetLastError());	
			closesocket(*connection);
			WSACleanup();
			return;
		}
	}
	while (result > 0);

}

void incrementMsgSequence() {
	msgSequence++;
}