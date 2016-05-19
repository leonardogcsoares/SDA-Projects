#include "stdafx.h"
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <ws2tcpip.h>



// Function declarations
void dealWithConnection(SOCKET * connection);


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "4660"

#define TAMMSGSETUP 44  // 5*8 caracteres + 4 separadores
#define TAMMSGACK   17  // 2*8 caracteres + 1 separador
#define TAMMSGREQ   17  // 2*8 caracteres + 1 separador
#define TAMMSGDADOS 44  // 5*8 caracteres + 4 separadores
#define TAMMSGSEGMENTSIZE 8 // 8 caracteres

#define MSG_SETUP_PREFIX	"00000001"
#define MSG_DATA_PREFIX		"00000005"
#define MSG_ACK_PREFIX		"00000002"

#define MSG_SEQUENCE_SETUP 0
#define MSG_SEQUENCE_DATA 1

int msgSequenceSetup	= 1;
int msgSequenceData		= 1;



void main(void) {
	int				result;
	
	WSADATA			wsaData;
	SOCKET			listeningSocket = INVALID_SOCKET;
	SOCKET			newConnection	= INVALID_SOCKET;
	SOCKADDR_IN		serverAddr;
	//SOCKADDR_IN		clientAddr;
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

	char cimentType[TAMMSGSEGMENTSIZE], 
		cimentToProduce[TAMMSGSEGMENTSIZE], 
		timeBeginProduction[TAMMSGSEGMENTSIZE],
		numberSequence[TAMMSGSEGMENTSIZE];

	int msgSequence, cimentTypeValue;
	float cimentToProduceValue;
	
	// Buffer for intermediating data
	char buf[DEFAULT_BUFLEN];

	// Predefined message responses for sending ACK or Data to client
	char msgAckResponse[TAMMSGACK+1] = "00000002|NNNNNNNN";
	char msgDataResponse[TAMMSGDADOS+1] = "00000005|NNNNNNNN|00000123|00001104|23:12:15";

	do {
		// Receives messages from clientConnections and returns error (-1) or bytes received
		result = recv(*connection, recvBuf, recvBufLen, 0);

		// Bytes were succesfully received
		if (result > 0) {
			printf("Bytes received: %d\n", result);
			printf("Message received: %s\n", recvBuf);
			printf("Size of message: %d\n", sizeof(recvBuf) / sizeof(recvBuf[0]));

			if(strncmp(&recvBuf[0], MSG_SETUP_PREFIX, 8) == 0) {
				// Setup message
				// Ler numero sequencial da mensagem
				memcpy_s(&numberSequence[0], 8, &recvBuf[9], 8) ;
				sscanf_s(numberSequence, "%d", &msgSequence);
				// Ler Tipo de Cimento
				memcpy_s(&cimentType[0], 8, &recvBuf[18], 8);
				sscanf_s(cimentType, "%d", &cimentTypeValue);
				// Ler Tonelagem a produzir
				memcpy_s(&cimentToProduce[0], 8, &recvBuf[27], 8);
				sscanf_s(cimentToProduce, "%f", &cimentToProduceValue);
				// Ler hora prevista de inicio
				memcpy_s(&timeBeginProduction[0], 8, &recvBuf[36], 8);


				printf("Number sequence: %d\n", msgSequence);
				printf("Type ciment: %d\n", cimentTypeValue);
				printf("Ciment to produce: %f\n", cimentToProduceValue);
				printf("Time to Begin Production: %8s\n", timeBeginProduction);

				// Chamar escrita sincrona, OPCClient

				// Montar ACK
				sprintf_s(buf, 10, "%08d", ++msgSequenceSetup);
				memcpy_s(&msgAckResponse[9], 8, buf, 8);
				// Mandar ACK para o client
				result = send(*connection, msgAckResponse, TAMMSGACK, 0);
				if (result == TAMMSGREQ)
					printf ("Msg de ACK enviada ao client TCP:\n%s\n\n", msgAckResponse);
			    else {
					if (result == 0) 
						// Connection destroyed
					    printf ("SEND retornou 0 bytes ... Falha!\n");
				    else if (result == SOCKET_ERROR){
						result=WSAGetLastError();
						printf("Falha no SEND: codigo de erro = %d\n", result);
					}
					break;
				}
			}
			else if(strncmp(&recvBuf[0], MSG_DATA_PREFIX, TAMMSGSEGMENTSIZE) == 0) {
				// Data message
				// Ler numero sequencial da mensagem
				memcpy_s(&numberSequence[0], 8, &recvBuf[9], 8) ;
				sscanf_s(numberSequence, "%d", &msgSequence);

				// Obter ultimos valores de dados de producao, OPCClient
				// Montar msgDadosProducao
				
				// Mandar mensagem dados para o client

			}


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