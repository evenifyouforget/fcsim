#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define bswap_64(x) _byteswap_uint64(x)
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <byteswap.h>
#endif

static int sock = -1;

int minit(const char *addr, int port)
{
	struct sockaddr_in saddr;
	int res;

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;
#endif

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return sock;

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &saddr.sin_addr) != 1)
		return -1;

	res = connect(sock, (struct sockaddr *)&saddr, sizeof(saddr));
	if (res < 0) {
		sock = -1;
		return res;
	}

	send(sock, "!c", 2, 0);

	return 0;
}

static char buf[1024];
static int buf_bytes;

static void buf_flush(void)
{
	int res;

	while (buf_bytes > 0) {
		res = send(sock, buf, buf_bytes, 0);
		if (res <= 0)
			buf_bytes = 0;
		else
			buf_bytes -= res;
	}
}

static void buf_write(const void *datav, int len)
{
	const char *data = (const char *)datav;
	int to_write;

	while (len > 0) {
		to_write = len;
		if (to_write > sizeof(buf) - buf_bytes)
			to_write = sizeof(buf) - buf_bytes;
		memcpy(buf + buf_bytes, data, to_write);
		buf_bytes += to_write;
		data += to_write;
		len -= to_write;
		if (buf_bytes == sizeof(buf))
			buf_flush();
	}
}

void mw(const char *name, int num_cnt, ...)
{
	va_list va;
	uint8_t name_size = strlen(name) + 1;
	uint8_t size = name_size + 8 * num_cnt;
	int i;

	if (sock < 0)
		return;

	buf_write(&size, 1);
	buf_write(name, name_size);

	va_start(va, num_cnt);
	for (i = 0; i < num_cnt; i++) {
		union {
			double d;
			uint64_t u;
		} num;
		num.d = va_arg(va, double);
		num.u = bswap_64(num.u);
		buf_write(&num, 8);
	}
}
