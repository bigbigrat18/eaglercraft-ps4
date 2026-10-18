#include <cstdio>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "local_http_server.h"
#include "log.h"

LocalHttpServer::LocalHttpServer()
	: port(0)
{
}

bool LocalHttpServer::Start(const char* app0RelativePath, uint16_t listenPort)
{
	FILE* f = fopen(app0RelativePath, "rb");
	if(!f)
	{
		DEBUGLOG << "[DEBUG] [ERROR] Failed to open " << app0RelativePath;
		return false;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(size <= 0)
	{
		fclose(f);
		DEBUGLOG << "[DEBUG] [ERROR] Asset file is empty: " << app0RelativePath;
		return false;
	}

	fileContent.resize(static_cast<size_t>(size));
	size_t readBytes = fread(&fileContent[0], 1, static_cast<size_t>(size), f);
	fclose(f);

	if(readBytes != static_cast<size_t>(size))
	{
		DEBUGLOG << "[DEBUG] [ERROR] Short read on " << app0RelativePath;
		return false;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] socket() failed";
		return false;
	}

	int reuse = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(listenPort);

	if(bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] bind() failed on 127.0.0.1:" << listenPort;
		close(sockfd);
		return false;
	}

	if(listen(sockfd, 4) != 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] listen() failed";
		close(sockfd);
		return false;
	}

	port = listenPort;

	// Detached: this server runs for the lifetime of the app (which never
	// returns), so there's no join point -- same spirit as the verified
	// threading sample's workers, made explicit rather than relying on
	// the thread object simply never going out of scope.
	std::thread serverThread([this, sockfd]()
	{
		for(;;)
		{
			sockaddr_in clientAddr;
			socklen_t addrLen = sizeof(clientAddr);
			int connfd = accept(sockfd, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
			if(connfd < 0)
				continue;

			// Not parsed -- every request gets the same single-page
			// response, so there's nothing to route on.
			char discard[2048];
			read(connfd, discard, sizeof(discard));

			char header[256];
			int headerLen = snprintf(header, sizeof(header),
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/html; charset=utf-8\r\n"
				"Content-Length: %zu\r\n"
				"Connection: close\r\n"
				"\r\n",
				fileContent.size());

			write(connfd, header, headerLen);

			// A single write() of ~34MB isn't guaranteed to send it all
			// in one call -- loop until everything's out.
			size_t sent = 0;
			while(sent < fileContent.size())
			{
				ssize_t n = write(connfd, fileContent.data() + sent, fileContent.size() - sent);
				if(n <= 0)
					break;
				sent += static_cast<size_t>(n);
			}

			close(connfd);
		}
	});
	serverThread.detach();

	return true;
}
