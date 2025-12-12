#include "TCP_Send.h"
#include "TCP_Buffer.h"
#include "TCP_Protocol.h"

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>


// 전체 데이터가 모두 전송될 때까지 반복
static bool SendAll(int sock, const void* data, size_t size)
{
    const char* pointer = (const char*)data;

    while (size > 0)
    {
        int sent = send(sock, pointer, size, 0);
        if (sent <= 0)
        {
            return false;
        }
        pointer += sent;
        size    -= sent;
    }
    return true;
}


void* TCP_Send_Thread(void* arg)
{
    int sock = *(int*)arg;

    if (sock < 0)
    {
        std::cerr << "[TCP] Invalid socket\n";
        pthread_exit(NULL);
    }

    std::cout << "[TCP] Send thread started (using main socket)\n";

    // 임시 저장 버퍼
    uint8_t Lepton_Frame[120][160][3];
    uint8_t RGB_Frame[640][640][3];

    while (1)
    {
        // 🔥 Lepton frame 송신
        if (TCP_Lepton_Buffer_Pop(Lepton_Frame))
        {
            FrameHeader header;
            header.type     = LEPTON_FRAME;
            header.width    = 160;
            header.height   = 120;
            header.channels = 3;
            header.dataSize = 160 * 120 * 3;

            SendAll(sock, &header, sizeof(FrameHeader));
            SendAll(sock, Lepton_Frame, header.dataSize);

        }

        // 🔥 RGB frame 송신
        if (TCP_RGB_Buffer_Pop(RGB_Frame))
        {
            FrameHeader header;
            header.type     = RGB_FRAME;
            header.width    = 640;
            header.height   = 640;
            header.channels = 3;
            header.dataSize = 640 * 640 * 3;

            SendAll(sock, &header, sizeof(FrameHeader));
            SendAll(sock, RGB_Frame, header.dataSize);

        }

        usleep(1000);  // 1ms 휴식 (과부하 방지)
    }

    close(sock);
    pthread_exit(NULL);
}
