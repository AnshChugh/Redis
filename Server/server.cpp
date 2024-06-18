#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"

#define DEFAULT_BUFLEN 512

int main() {
	WSADATA wsaData;

	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0) {
		printf("WSAStartup Failed : %d\n", iResult);
		return 1;
	}


	struct addrinfo* result = NULL, * ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);

	if (iResult != 0) {
		printf("getaddressinfo failed %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET ListenSocket = INVALID_SOCKET;

	// Create a socket to listen for client Connection
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);


	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult != 0) {
		printf("bind failed with error %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed with error %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}


	SOCKET ClientSocket;

	ClientSocket = INVALID_SOCKET;

	// accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed: %ld\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}



	char recvbuf[DEFAULT_BUFLEN];
	int  iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);

			// Echo the buffer back to the sender
			iSendResult = send(ClientSocket, recvbuf, iResult, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}
			printf("Bytes sent: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

	} while (iResult > 0);


	return 0;
}