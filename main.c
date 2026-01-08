#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/stat.h>


int main(void)
{
	const int BUFFER_SIZE = 2048, PORT = 8080;
	int running = 1;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) 
	{ 
		perror("socket"); 
		return 1; 
	}

	int opt = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
	{
        perror("bind"); return 1;
    }

	printf("Server up and running on port %i\n", PORT);

    listen(s, 16);

	char buffer[BUFFER_SIZE];

	while (running)
	{
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = accept(s, (struct sockaddr*)&client_addr, &client_len);

		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
		int port = ntohs(client_addr.sin_port);

		printf("Client connected from %s:%d\n", ip, port);

		if (client_fd < 0) 
		{ 
			perror("accept"); 
			return 1; 
		}
	
		int n = recv(client_fd, buffer, BUFFER_SIZE, 0);
		if (n <= 0)
		{
			close(client_fd);
			continue;
		}
		buffer[n] = 0;

		if(strncmp(buffer, "GET ", 4) != 0)
		{
			close(client_fd);
			continue;
		}

		char *path_to_file = buffer + 4;
		char *space = strchr(path_to_file, ' ');
		if (!space)
		{
			close(client_fd);
			continue;
		}
		*space = 0;

		if (strstr(path_to_file, ".."))
		{
			close(client_fd);
			continue;
		}

		if (*path_to_file == '/') path_to_file++;

		int fd = open(path_to_file, O_RDONLY);
		if (fd < 0) {
			const char *msg =
				"HTTP/1.1 404 Not Found\r\n"
				"Content-Length: 0\r\n\r\n";
			send(client_fd, msg, strlen(msg), 0);
			close(client_fd);
			continue;
		}

		struct stat st;
		fstat(fd, &st);

		char header[256];
		int hlen = snprintf(header, sizeof(header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: %ld\r\n"
			"Content-Type: application/octet-stream\r\n"
			"\r\n",
			st.st_size);
		
		sendfile(client_fd, fd, NULL, st.st_size);

		close(fd);
		close(client_fd);

	}

    
}
