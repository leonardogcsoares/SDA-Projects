// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE�s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <ObjIdl.h>

#include "opcda.h"
#include "opcerror.h"
#include "SimpleOPCClient_v3.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

#include "stdafx.h"
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <ws2tcpip.h>

using namespace std;

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
//#define VT VT_R4



// Function declarations
void tcpServer();
void dealWithConnection(SOCKET * connection);
void opcClient(void* ignored);

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


//#define REMOTE_SERVER_NAME L"your_path"

// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

wchar_t ITEM_PRODUCTION[]=L"Random.Int2";
wchar_t ITEM_OEE[]=L"Random.Real4";
wchar_t ITEM_TIME_FINISH[]=L"Random.Time";



SOCAdviseSink* gSOCAdviseSink;

//////////////////////////////////////////////////////////////////////
// Read the value of an item on an OPC server.
//
void main(void) {

	_beginthread(opcClient, 0, NULL);
	Sleep(3000L);
	tcpServer();




}


void opcClient(void* ignored)
{
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup; // server handle to the group
	OPCHANDLE hServerItemProduction;  // server handle to the item
	OPCHANDLE hServerItemOEE;  // server handle to the item
	OPCHANDLE hServerItemTimeFinish;  // server handle to the item

	int i;
	char buf[300];

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitialize(NULL);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Intantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);

	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	printf("Adding a group in the INACTIVE state for the moment...\n");
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC item. First we have to convert from wchar_t* to char*
	// in order to print the item name in the console.
    size_t m;
	wcstombs_s(&m, buf, 100, ITEM_PRODUCTION, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItemProduction, ITEM_PRODUCTION, VT_I2, 0);
	Sleep(500L);
	wcstombs_s(&m, buf, 100, ITEM_OEE, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItemOEE, ITEM_OEE, VT_R4, 1);
	Sleep(500L);
	wcstombs_s(&m, buf, 100, ITEM_TIME_FINISH, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItemTimeFinish, ITEM_TIME_FINISH, VT_R8, 2);
	Sleep(500L);

	// Establish a callback asynchronous read by means of the old IAdviseSink()
	// (OPC DA 1.0) method. We first instantiate a new SOCAdviseSink object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.
	IDataObject* pIDataObject = NULL; //pointer to IDataObject interface
	DWORD tkAsyncConnection = 0;
	SOCAdviseSink* pSOCAdviseSink = new SOCAdviseSink ();
	pSOCAdviseSink->AddRef();
    printf("Setting up the IAdviseSink callback connection...\n");
    SetAdviseSink(pIOPCItemMgt, pSOCAdviseSink, pIDataObject, &tkAsyncConnection);
	gSOCAdviseSink = pSOCAdviseSink;

	// Change the group to the ACTIVE state so that we can receive the
	// server�s callback notification
	printf("Changing the group state to ACTIVE...\n");
    SetGroupActive(pIOPCItemMgt);

	// Enters a message pump in order to process the server�s callback
	// notifications. This is needed because the CoInitialize() function
	// forces the COM threading model to STA (Single Threaded Apartment),
	// in which, according to the MSDN, "all method calls to a COM object
	// (...) are synchronized with the windows message queue for the
	// single-threaded apartment's thread." So, even being a console
	// application, the OPC client must process messages (which in this case
	// are only useless WM_USER [0x0400] messages) in order to process
	// incoming callbacks from a OPC server.
	//
	// A better alternative could be to use the CoInitializeEx() function,
	// which allows one to  specifiy the desired COM threading model;
	// in particular, calling
	//        CoInitializeEx(NULL, COINIT_MULTITHREADED)
	// sets the model to MTA (MultiThreaded Apartments) in which a message
	// loop is __not required__ since objects in this model are able to
	// receive method calls from other threads at any time. However, in the
	// MTA model the user is required to handle any aspects regarding
	// concurrency, since asynchronous, multiple calls to the object methods
	// can occur.
	//
	int bRet;
	MSG msg;
	DWORD ticks1, ticks2;
	ticks1 = GetTickCount();
	printf("Waiting for IAdviseSink callback notifications during 10 seconds...\n");
	do {
		bRet = GetMessage( &msg, NULL, 0, 0 );
		if (!bRet){
			printf ("Failed to get windows message! Error code = %d\n", GetLastError());
			exit(0);
		}
		TranslateMessage(&msg); // This call is not really needed ...
		DispatchMessage(&msg);  // ... but this one is!
        ticks2 = GetTickCount();
	}
	while (1);

	// Cancel the callback and release its reference
	printf("Cancelling the IAdviseSink callback...\n");
    CancelAdviseSink(pIDataObject, tkAsyncConnection);
	pSOCAdviseSink->Release();

	// Remove the OPC item:
	printf("Removing the OPC items...\n");
	RemoveItem(pIOPCItemMgt, hServerItemProduction);
	RemoveItem(pIOPCItemMgt, hServerItemOEE);
	RemoveItem(pIOPCItemMgt, hServerItemTimeFinish);

	// Remove the OPC group:
	printf("Removing the OPC group object...\n");
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	printf("Removing the OPC server object...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf ("Releasing the COM environment...\n");
	CoUninitialize();
}

////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //};

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer.
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt,
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// Add an OPC group and get a pointer to the IUnknown I/F:
    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}

//////////////////////////////////////////////////////////////////
// Add the Item ITEM_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.

void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem, LPWSTR ITEM, int VT, int hClient)
{
	HRESULT hr;

	// Array of items to add:
	OPCITEMDEF ItemArray[1] =
	{{
	/*szAccessPath*/ L"",
	/*szItemID*/ ITEM,
	/*bActive*/ TRUE,
	/*hClient*/ hClient,
	/*dwBlobSize*/ 0,
	/*pBlob*/ NULL,
	/*vtRequestedDataType*/ VT,
	/*wReserved*/0
	}};

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(1, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem = pAddResult[0].hServer;

	// release memory allocated by the server:
	CoTaskMemFree(pAddResult->pBlob);

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue.
//
void WriteItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Write(1, &hServerItem, &varValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);


	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;

	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(1, hServerArray, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}

void RetrieveValues(char *production, char *oee, char *time) {
	char prodAux[100], oeeAux[100], timeAux[100];
	gSOCAdviseSink->RetrieveValues(prodAux, oeeAux, timeAux);
	strcpy(production, prodAux);
	strcpy(oee, oeeAux);
	strcpy(time, timeAux);
}


/*//////////////////////////////////////////////////////////////////
		TCP SERVER SOCKET
//////////////////////////////////////////////////////////////////*/

void tcpServer() {
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
	char msgDataResponse[TAMMSGDADOS+1] = "00000010|NNNNNNNN|00000123|00001104|23:12:15";

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
				char production[100];
				char oee[100];
				char time[100];
				RetrieveValues(production, oee, time);
				printf("\n%s, %s, %s\n\n", production, oee, time);
				//printf("\n%lf,  %lf", atof(oee), std::stof(oee));
								
				// Montar msgDadosProducao
				sprintf_s(buf, 10, "%08d", ++msgSequenceData);
				memcpy_s(&msgDataResponse[9], 8, buf, 8);
				sprintf_s(buf, 10, "%08d", std::stoi(production));
				memcpy_s(&msgDataResponse[18], 8, buf, 8);
				sprintf_s(buf, 10, "%08.2f", atof(oee));
				memcpy_s(&msgDataResponse[27], 8, buf, 8);
				sprintf_s(buf, 10, "%02d", rand()%24);
				memcpy_s(&msgDataResponse[36], 2, buf, 2);
				sprintf_s(buf, 10, "%02d", rand()%60);
				memcpy_s(&msgDataResponse[39], 2, buf, 2);
				sprintf_s(buf, 10, "%02d", rand()%60);
				memcpy_s(&msgDataResponse[42], 2, buf, 2);
				printf("Message response: %s", msgDataResponse);
				// Mandar mensagem dados para o client
				result = send(*connection, msgDataResponse, TAMMSGDADOS, 0);
				printf("\n\n%d", result);
				if (result == TAMMSGDADOS)
					printf ("Msg de Dados enviada ao client TCP:\n%s\n\n", msgDataResponse);
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
