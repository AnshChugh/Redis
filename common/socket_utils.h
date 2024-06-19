#include <WinSock2.h>
#include <WS2tcpip.h>
#include <cstdint>

// Since recv and send does not send or recieve all bytes at once  we created these helper functions
// thanks to https://stackoverflow.com/questions/27527395/how-to-get-the-exact-message-from-recv-in-winsock-programming

static int32_t read_full(SOCKET s, char* buf, int len) {
	int total = 0;
	int bytes_left = len;
	int n = -1;

	while (total < len) {
		n = recv(s, buf + total, bytes_left, 0);
		if (n <= 0) break;
		total += n;
		bytes_left -= n;
	}
	return (n <= 0) ? SOCKET_ERROR : 0;
}

static int32_t write_full(SOCKET s, char* buf, int len) {
	int total = 0;
	int bytes_left = len;
	int n = -1;

	while (total < len) {
		n = send(s, buf + total, bytes_left, 0);
		if (n <= 0) break;
		total += n;
		bytes_left -= n;
	}
	return (n <= 0) ? SOCKET_ERROR : 0;
}
