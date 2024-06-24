#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <vector>
#include "../common/socket_utils.h"
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"

#define DEFAULT_BUFLEN 512

const size_t k_max_msg = 4096;


// for multiple connections
enum {
	STATE_REQ = 0,
	STATE_RES = 1,
	STATE_END = 2,  // mark the connection for deletion
};

struct Conn {
	SOCKET fd = -1;
	uint32_t state = 0;     // either STATE_REQ or STATE_RES
	// buffer for reading
	size_t rbuf_size = 0;
	uint8_t rbuf[4 + k_max_msg];
	// buffer for writing
	size_t wbuf_size = 0;
	size_t wbuf_sent = 0;
	uint8_t wbuf[4 + k_max_msg];
};

int InitSocketLib();
static int32_t one_request(SOCKET);
static void fd_set_nb(SOCKET);
static void connection_io(Conn*);
static int32_t accept_new_conn(std::vector<Conn*>&, SOCKET);
static void connection_io(Conn*);
static void state_req(Conn*);
static void state_res(Conn*);
static bool try_fill_buffer(Conn*);
static bool try_one_request(Conn*);
static bool try_flush_buffer(Conn*);

int main() {
	int iResult;

	// init
	if (iResult = InitSocketLib() != 0) {
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
	// a map of all client connections, keyed by fd
	std::vector<Conn*> fd2conn;

	fd_set_nb(ListenSocket);
	
	
	// event loop
	std::vector<WSAPOLLFD> poll_args;
	while (true) {
		poll_args.clear();
		// prepare argument of poll()
		// for convenience, the listening fd is put in the first position
		WSAPOLLFD pfd=  { ListenSocket, POLLIN, 0 };
		poll_args.push_back(pfd);
		// connection sockets
		for (Conn* conn : fd2conn) {
			if (!conn) continue;

			pfd = {};
			pfd.fd = conn->fd;
			pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
			pfd.events = pfd.events | POLLERR;
			poll_args.push_back(pfd);
		}


		// poll for active fds
		// the timeout argument doesn't matter here
		iResult = WSAPoll(poll_args.data(), poll_args.size(), 1000);
		if (iResult == SOCKET_ERROR) {
			printf("some error occured while WSAPoll. error: %d\n", WSAGetLastError());
			break;
		}

		//process active connections
		for (size_t i = 1; i < poll_args.size(); i++) {
			if (poll_args[i].revents) {
				Conn* conn = fd2conn[poll_args[i].fd];
				connection_io(conn);
				if (conn->state == STATE_END) {
					// client closed normally or something bad happened
					// destroy this connection;
					fd2conn[conn->fd] = NULL;
					closesocket(conn->fd);
					free(conn);
				}
			}
		}

		// try to accept new connections if listing socket is active
		if (poll_args[0].revents) {
			accept_new_conn(fd2conn, ListenSocket);
		}
	}

	// no need to listen now
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}

int InitSocketLib() {
	WSADATA wsaData;
	int errCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	return errCode;
}

// set to non blocking io
static void fd_set_nb(SOCKET fd) {
	u_long mode = 1;  // 1 to enable non-blocking socket
	int result = ioctlsocket(fd, FIONBIO, &mode);
	if (result == SOCKET_ERROR) {
		printf("Error setting to non-blocking mode\n");
	}
}



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



static void conn_put(std::vector<Conn*>& fd2conn, struct Conn* conn) {
	if (fd2conn.size() <= (size_t)conn->fd) {
		fd2conn.resize(conn->fd + 1);
	}
	fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn*> &fd2conn, SOCKET fd) {
	// accept
	SOCKET ClientSocket = accept(fd, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed: %ld\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return -1;
	}

	// set new connection to non blocking mode
	fd_set_nb(ClientSocket);

	// creating new connection  struct
	Conn* conn = (Conn*)malloc(sizeof(Conn));
	if (!conn) {
		closesocket(ClientSocket);
		return -1;
	}

	conn->fd = ClientSocket;
	conn->state = STATE_REQ;
	conn->rbuf_size = 0;
	conn->wbuf_size = 0;
	conn->wbuf_sent = 0;
	conn_put(fd2conn, conn);
	return 0;
}


static void connection_io(Conn* conn) {
	if (conn->state == STATE_REQ) {
		state_req(conn);
	}
	else if (conn->state == STATE_RES) {
		state_res(conn);
	}
	else {
		printf("Very Unexpected\n");// not expected
	}
}


static void state_req(Conn* conn) {
	while (try_fill_buffer(conn)) {

	}
}

static bool try_fill_buffer(Conn* conn) {
	// try to fill buffer	
	assert(conn->rbuf_size < sizeof(conn->rbuf));
	int rv = 0;
	do {
		size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
		rv = recv(conn->fd, (char*) & conn->rbuf[conn->rbuf_size], cap, 0);
	} while (rv < 0 && errno == EAGAIN);
	if (rv < 0 && errno == EAGAIN) {
		// got EAGAIN, stop.
		return false;
	}
	if (rv < 0) {
		printf("read() error\n");
		conn->state = STATE_END;
		return false;
	}
	if (rv == 0) {
		if (conn->rbuf_size > 0) {
			printf("unexpected EOF\n");
		}
		else {
			printf("EOF\n");
		}
		conn->state = STATE_END;
		return false;
	}
	conn->rbuf_size += (size_t)rv;
	assert(conn->rbuf_size <= sizeof(conn->rbuf));

	// Try to process requests one by one.
	// Why is there a loop? Please read the explanation of "pipelining".
	while (try_one_request(conn)) {}
	return (conn->state == STATE_REQ);
}

// explaining the loop here

//clients can save some latency by sending multiple requests without 
// waiting for responses in between, this mode of operation is called “pipelining”.
// Thus we can’t assume that the read buffer contains at most one request

static bool try_one_request(Conn* conn) {

	// try to parse one request from the buffer
	if (conn->rbuf_size < 4) {
		// not enough data in buffer. Will try in the next iteration
	}
	uint32_t len = 0;
	memcpy(&len, &conn->rbuf[0], 4);
	if (len > k_max_msg) {
		printf("msg too long!\n");
		return false;
	}

	if (len + 4 > conn->rbuf_size) {
		// not enough data in buffer. Will retry in next iteration
		return false;
	}
	
	// got one request, do something with it
	printf(" Client says %.*s\n", &conn->rbuf[4]);
	conn->wbuf_size = 4 + len;

	// remove request from buffer
	// Note: frequent memmove is inefficient
	// note: need better handling for production code

	size_t remain = conn->rbuf_size - 4 - len;
	if (remain) {
		memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
	}
	conn->rbuf_size = remain;

	//change state
	conn->state = STATE_RES;
	state_res(conn);

	// continue the outer loop if the request was fully processed
	return (conn->state == STATE_REQ);
}

static void state_res(Conn* conn) {
	while (try_flush_buffer(conn)) {}
}

static bool try_flush_buffer(Conn* conn) {
	int iResult = 0;
	do {
		size_t remain = conn->wbuf_size - conn->wbuf_sent;
		iResult = send(conn->fd, (char*) & conn->wbuf[conn->wbuf_sent], remain, 0);
	} while (iResult < 0 && errno == EINTR);
	if (iResult < 0 && errno == EAGAIN) {
		// got EGAIN , stop
		return false;
	}
	if (iResult < 0) {
		printf("send() error : %d\n", WSAGetLastError());
	}

	conn->wbuf_sent += (size_t)iResult;
	assert(conn->wbuf_sent <= conn->wbuf_size);
	if (conn->wbuf_sent == conn->wbuf_size) {
		// buffer fully sent, change STATE back	
		conn->state = STATE_REQ;
		conn->wbuf_sent = 0;
		conn->wbuf_size = 0;
		return false;
	}

	// still got some data left, continue loop
	return true;
}

