#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <err.h>
#include <poll.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


#define SOCKETS_MAX 128
#define POLL_WAITMS 1000
#define BUFFER_SIZE 512

#define lengthof(X) (sizeof(X) / sizeof(*(X)))

int main(int argc, char **argv)
{
	int res;

	int peers[SOCKETS_MAX] = {0};
	struct pollfd ppeers[SOCKETS_MAX] = {0};
	char buf[BUFFER_SIZE] = {0};

	int port = 8604;
	if (argc >= 2)
		port = atoi(argv[1]);

	struct in_addr inaddr = (struct in_addr)
	{ .s_addr = INADDR_ANY };
	if (argc >= 3)
	{
		res = inet_pton(AF_INET, argv[2], &inaddr);
		if (res == -1)
			err(1, "inet_pton()");
	}

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock == -1)
		err(1, "socket()");

	res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if (res == -1)
		err(1, "setsockopt()");

	struct sockaddr_in addr = (struct sockaddr_in)
	{
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = inaddr,
	};
	res = bind(sock, (void *)&addr, sizeof(addr));
	if (res == -1)
		err(1, "bind()");

	res = listen(sock, 16);
	if (res == -1)
		err(1, "listen()");
	
	signal(SIGPIPE, SIG_IGN);
	for (;;)
	{
		int client = accept(sock, NULL, NULL);
		if (client > 0)
		{
			for (size_t i = 0; i < lengthof(peers); ++i)
				if (!peers[i])
				{
					peers[i] = client;
					goto accept_end;
				}
			warnx("dropping %d as peer list is full", client);
			close(client);
		}
accept_end:;
		nfds_t written = 0;
		for (size_t i = 0; i < lengthof(peers); ++i)
			if (peers[i])
				ppeers[written++] = (struct pollfd)
				{
					.fd = peers[i],
					.events = POLLIN,
				};
		res = poll(ppeers, written, POLL_WAITMS);
		if (res == -1)
			err(1, "poll");
		if (!res)
			continue;

		for (size_t i = 0; i < written; ++i)
		{
			if (!(ppeers[i].revents & POLLIN))
				continue;

			ssize_t reads = read(ppeers[i].fd, buf, sizeof(buf));
			if (reads == -1 || !reads)
			{
				for (size_t j = 0; j < lengthof(peers); ++j)
					if (ppeers[i].fd == peers[j])
					{
						close(peers[j]);
						peers[j] = 0;
						break;
					}
				continue;
			}

			for (size_t j = 0; j < lengthof(peers); ++j)
			{
				if (!peers[j] || i == j)
					continue;
				size_t writtenall = 0;
				do
				{
					ssize_t written = write(peers[j], buf, reads);
					if (written == -1)
						break;
					else
						writtenall += written;
				} while (writtenall < reads);
			}
		}
	}
}
