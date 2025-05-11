#include <errno.h>
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
#define BUFFER_SIZE 512

#define lengthof(X) (sizeof(X) / sizeof(*(X)))

int main(int argc, char **argv)
{
	struct sockaddr_in addr = {.sin_family = AF_INET};
	int peers[SOCKETS_MAX] = {0}, res;
	struct pollfd ppeers[SOCKETS_MAX] = {0};
	char buf[BUFFER_SIZE] = {0};

	addr.sin_port = htons(8604);
	if (argc >= 2)
		addr.sin_port = htons(atoi(argv[1]));

	addr.sin_addr = (struct in_addr){.s_addr = INADDR_ANY};
	if (argc >= 3)
	{
		res = inet_pton(AF_INET, argv[2], &addr.sin_addr);
		if (res == -1)
			err(1, "inet_pton()");
	}

	peers[0] = socket(AF_INET, SOCK_STREAM, 0);
	if (peers[0] == -1)
		err(1, "socket()");
	res = setsockopt(peers[0], SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if (res == -1)
		err(1, "setsockopt()");
	res = bind(peers[0], (void *)&addr, sizeof(addr));
	if (res == -1)
		err(1, "bind()");
	res = listen(peers[0], 16);
	if (res == -1)
		err(1, "listen()");
	
	signal(SIGPIPE, SIG_IGN);
	for (;;)
	{
		nfds_t written = 0;
		for (size_t i = 0; i < lengthof(peers); ++i)
			if (peers[i])
				ppeers[written++] = (struct pollfd)
				{
					.fd = peers[i],
					.events = POLLIN,
				};
		res = poll(ppeers, written, -1);
		if (res == -1)
			err(1, "poll");

		if (ppeers[0].revents & POLLIN)
		{
			int client = accept(peers[0], NULL, NULL);
			if (client == -1)
			{
				warn("accept()");
				goto after;
			}
			for (size_t i = 1; i < lengthof(peers); ++i)
				if (!peers[i])
				{
					peers[i] = client;
					goto after;
				}
			warnx("dropping %d as peer list is full", client);
			close(client);
		}
after:
		for (size_t i = 1; i < written; ++i)
		{
			if (!(ppeers[i].revents & POLLIN))
				continue;

			ssize_t reads = read(ppeers[i].fd, buf, sizeof(buf));
			if (reads == -1 || !reads)
			{
				for (size_t j = 1; j < lengthof(peers); ++j)
					if (ppeers[i].fd == peers[j])
					{
						close(peers[j]);
						peers[j] = 0;
						break;
					}
				continue;
			}

			for (size_t j = 1; j < lengthof(peers); ++j)
				if (peers[j] && i != j)
					while (errno = 0, write(peers[j], buf, reads), errno == EINTR);
		}
	}
}
