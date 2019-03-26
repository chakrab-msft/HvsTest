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

using namespace std::chrono_literals;

std::atomic<int> msgCount = 0;

#define DEFAULT_BUFFER_LENGTH 1000
#define SEND_BUFFER_LENGTH 4 * 1024 * 1024
#define AF_VSOCK 40

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

typedef struct _SOCKADDR_HV
{
    unsigned short Family;
    unsigned short Reserved;
    GUID VmId;
    GUID ServiceId;
} SOCKADDR_HV;

typedef struct _SOCKADDR_VM
{
    unsigned short Family;
    unsigned short Reserved;
    unsigned int SvmPort;
    unsigned int SvmCID;

    unsigned char svm_zero[sizeof(struct sockaddr) -
        sizeof(sa_family_t) - sizeof(unsigned short) -
        sizeof(unsigned int) - sizeof(unsigned int)];
} SOCKADDR_VM;

#define DEFINE_GUID1(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_GUID1(HV_GUID_PARENT,
    0xa42e7cda, 0xd03f, 0x480c, 0x9c, 0xc2, 0xa4, 0xde, 0x20, 0xab, 0xb8, 0x78);

/* 3049197C-FACB-11E6-BD58-64006A7986D3 */
DEFINE_GUID1(SERVICE_GUID,
    0x3049197c, 0xfacb, 0x11e6, 0xbd, 0x58, 0x64, 0x00, 0x6a, 0x79, 0x86, 0xd3);

#define SERVICE_PORT 0x3049197c

#define INVALID_SOCKET 0
#define VMADDR_CID_HOST 2

class Client {
public:
    Client()
    {
        ConnectSocket = INVALID_SOCKET;
    }

    bool Start()
    {
        SOCKADDR_HV clientService;
        GUID VmID, ServiceID;

        ConnectSocket = socket(AF_VSOCK, SOCK_STREAM, 0);

        if (ConnectSocket == INVALID_SOCKET)
        {
            fprintf(stderr, "Socket Error: %d. %s\n", errno, strerror(errno));
            return false;
        }

        // Connect to server
        SOCKADDR_VM savm;
        memset(&savm, 0, sizeof(savm));
        savm.Family = AF_VSOCK;
        savm.SvmCID = VMADDR_CID_HOST;
        savm.SvmPort = SERVICE_PORT;

        int iResult = connect(ConnectSocket, (const struct sockaddr *)&savm, sizeof(savm));

        if (iResult == SOCKET_ERROR)
        {
            fprintf(stderr, "Connect Error: %d. %s\n", errno, strerror(errno));
            return false;
        }

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

    // Receive message from server
    bool Recv(char* recvbuf, int len)
    {
        int iResult = recv(ConnectSocket, recvbuf, len, 0);
        if (iResult > 0)
        {
            return true;
        }


        return false;
    }

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

    char* sendbuf = new char[SEND_BUFFER_LENGTH];

    for (int i = 0; i < 99999999; ++i)
    {
        client.Send(sendbuf, SEND_BUFFER_LENGTH);
    }

    client.Stop();
    return 0;
}