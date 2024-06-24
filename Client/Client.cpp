#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <cstdint>
#include "../common/socket_utils.h"

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"

#define DEFAULT_BUFLEN 512

const size_t k_max_msg = 4096;

static int32_t send_req(SOCKET, const char*);
static int32_t  read_res(SOCKET);


int main(int argc, char *argv[]) {

	WSADATA wsaData;

	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0) {
		printf("WSAStartup Failed : %d\n", iResult);
		return 1;
	}

	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET ConnectSocket = INVALID_SOCKET;

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
		ptr->ai_protocol);


	if (ConnectSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// connect to server
	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}

	freeaddrinfo(result);

	const char* query_list[3] = { "hello1", "hello2" , "hello3" };
	
	for (size_t i = 0; i < 3; i++) {
		iResult = send_req(ConnectSocket, query_list[i]);
		if (iResult == SOCKET_ERROR) {
			goto CLEANUP;
		}
	}
	for (size_t i = 0; i < 3; i++) {
		iResult = read_res(ConnectSocket);
		if (iResult == SOCKET_ERROR) {
			goto CLEANUP;
		}
	}

	CLEANUP:
	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}


// protocol 4-byte little endian inteeger indicating length of request , followed by 
// variable-length request

static int32_t send_req(SOCKET socket, const char* text) {
	uint32_t len = (uint32_t)strlen(text);
	if (len > k_max_msg) {
		printf("Error: too long \n");
		return SOCKET_ERROR;
	}

	char wbuf[4 + k_max_msg];
	// write using format length followed by data
	memcpy(wbuf, &len, 4); // assume little endian
	memcpy(&wbuf[4], text, len);

	// returns -1 if unsuccessful
	int32_t err = write_full(socket, wbuf, len + 4);
	if (err == SOCKET_ERROR) {
		printf("write error\n");
		return err;
	}
	return 0;
}
static int32_t read_res(SOCKET socket) {
	uint32_t len;
	// 4 bytes header
	char rbuf[4 + k_max_msg + 1];
	errno = 0;
	int32_t err = read_full(socket, rbuf, 4);
	if (err == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	memcpy(&len, rbuf, 4); // assume little endian
	if (len > k_max_msg) {
		printf("error: too long\n");
		return SOCKET_ERROR;
	}

	// reply body
	err = read_full(socket, &rbuf[4], len);
	if (err == SOCKET_ERROR) return SOCKET_ERROR;

	// do something
	rbuf[4 + len] = '\0';
	printf("server says: %s\n", &rbuf[4]);

	return 0;
}