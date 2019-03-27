#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <netinet/in.h>
#include <netdb.h>

using namespace std::chrono_literals;

std::atomic<int> msgCount {};

#define SEND_BUFFER_LENGTH 4 * 1024 * 1024

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

#define DEFINE_GUID1(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
         const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }


#define SERVICE_PORT 0x3049197c

#define INVALID_SOCKET -1

typedef int SOCKET;
#define SOCKET_ERROR -1
#define closesocket(_fd) close(_fd)
#define SD_SEND SHUT_WR

class Client {
public:
    Client()
    {
        ConnectSocket = INVALID_SOCKET;
    }

    bool Start()
    {
        ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (ConnectSocket == INVALID_SOCKET)
        {
            fprintf(stderr, "Socket Error: %d. %s\n", errno, strerror(errno));
            return false;
        }

        // Connect to server
	hostent *server = gethostbyname("192.168.0.1");
	if (!server)
	{
	    fprintf(stderr, "No such host\n");
	    return false;
	}

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
	addr.sin_port = htons(12999);
	memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

	std::cout << "Connecting..." << std::endl;
        int iResult = connect(ConnectSocket, (const struct sockaddr *)&addr, sizeof(addr));

        if (iResult == SOCKET_ERROR)
        {
            fprintf(stderr, "Connect Error: %d. %s\n", errno, strerror(errno));
            return false;
        }

	std::cout << "Connected!" << std::endl;
        return true;
    };

    // Free the resouces
    void Stop()
    {
        int iResult = shutdown(ConnectSocket, SD_SEND);
        if (iResult == SOCKET_ERROR)
        {
            fprintf(stderr, "shutdown Error: %d. %s\n", errno, strerror(errno));
        }

        closesocket(ConnectSocket);
    };

    // Send message to server
    bool Send(char* msg, int len)
    {
        int iResult = send(ConnectSocket, msg, len, 0);
        if (iResult == SOCKET_ERROR)
        {
            fprintf(stderr, "send Error: %d. %s\n", errno, strerror(errno));
            Stop();
            return false;
        }

        ++msgCount;
        return true;
    };


private:
    SOCKET ConnectSocket;
};


int main(int argc, char* argv[])
{
    std::string msg;

    Client client;

    if (!client.Start())
        return 1;

    std::thread t([&]
        {
            while (true)
            {
                std::this_thread::sleep_for(10s);
                std::cout << "Throughput " << msgCount / 10 << " req/sec" << std::endl;
                msgCount = 0;
            }
        });

    t.detach();

    std::vector<char> sendbuf(SEND_BUFFER_LENGTH);
    for (int i = 0; i < 99999999; ++i)
    {
        if(!client.Send(sendbuf.data(), SEND_BUFFER_LENGTH))
	{
            std::cout << "Send failed at loop " << i << std::endl;
            break;
	}
    }

    client.Stop();
    return 0;
}
