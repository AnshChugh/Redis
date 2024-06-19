#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <cstdint>
#include "../common/socket_utils.h"
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"

#define DEFAULT_BUFLEN 512

const size_t k_max_msg = 4096;


// protocol 4-byte little endian inteeger indicating length of request , followed by 
// variable-length request

static int32_t one_request(SOCKET connectionSocket) {
	// 4bytes header
	char rbuf[4 + k_max_msg + 1]; // buffer large enough to hold one request;

	int32_t err = read_full(connectionSocket, rbuf, 4);
	if (err == SOCKET_ERROR) {
		printf("read error: %d\n", WSAGetLastError());
		return err;
	}

	// change the given length from buffer to int
	uint32_t len = 0;
	memcpy(&len, rbuf, 4); // assume little endian

	if (len > k_max_msg) {
		printf("too long\n");
		return -1;
	}

	// request body  (data bytes)
	err = read_full(connectionSocket, &rbuf[4], len);
	if (err == SOCKET_ERROR) {
		printf("read error\n");
	}

	// do something
	rbuf[4 + len] = '\0'; // added null termination so that printf does not print unneccesary garbage
	printf("Client Says : %s\n", &rbuf[4]);


	// reply using the same protocol;
	const char reply[] = "Hi, from Server";
	char wbuf[4 + sizeof(reply)];
	len = (uint32_t)strlen(reply);
	memcpy(wbuf, &len, 4); // write the lengh as bytes
	memcpy(&wbuf[4], reply, len); // write the data as bytes
	
	return write_full(connectionSocket, wbuf, 4 + len);
}



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

	// handle multiple requests
	while (true) {
		// accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed: %ld\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		// only serves one client at a time
		while (true) {
			int32_t err = one_request(ClientSocket);
			if (err == SOCKET_ERROR) {
				break;
			}
		}

		// shutdown the send half of the connection since no more data will be sent
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

		// closesocket
		closesocket(ClientSocket);
	}

	// no need to listen now
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}