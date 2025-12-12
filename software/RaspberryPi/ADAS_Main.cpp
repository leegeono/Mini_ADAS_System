#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>

#include "3d/Lane3D.h"
#include "3d/Object3D.h"

#include "TCP_Pi/TCP_Buffer.h"
#include "TCP_Pi/TCP_Receive.h"
#include "TCP_Pi/TCP_Send.h"

#include "LeptonView/LeptonSPI.h"
#include "LeptonView/Lepton.h"
#include "RGBView/RGB.h"

extern void* ReadThread(void*);
extern void* ProcessThread(void*);
extern void* RGB_Thread(void*);
extern void* TCP_Send_Thread(void*);
extern void* TCP_ReceiveThread(void*);

volatile int running = 1;


void sig(int)
{
    running = 0;
    SpiClosePort();
    exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGINT, sig);
    PacketBuffer_Init(&PacketBuffer);

    // --------------------------------------------
    // 🔥 Lepton SPI 초기화
    // --------------------------------------------
    SpiOpenPort();
    Lepton_Reset();
    usleep(400000);

    // --------------------------------------------
    // 🔥 TCP Connect
    // --------------------------------------------
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in s{};
    s.sin_family = AF_INET;
    s.sin_port   = htons(5000);
    inet_pton(AF_INET, "192.168.10.1", &s.sin_addr);

    if (connect(sock, (sockaddr*)&s, sizeof(s)) != 0)
    {
        printf("[ERR] TCP connect failed\n");
        return 0;
    }

    // --------------------------------------------
    // 🔥 Thread 시작
    // --------------------------------------------
    pthread_t t_read, t_proc, t_rgb, t_send, t_recv;

    pthread_create(&t_read,  NULL, ReadThread,      NULL);
    pthread_create(&t_proc,  NULL, ProcessThread,   NULL);
    pthread_create(&t_rgb,   NULL, RGB_Thread,      NULL);
    pthread_create(&t_send,  NULL, TCP_Send_Thread, &sock);
    pthread_create(&t_recv,  NULL, TCP_ReceiveThread, &sock);

    // --------------------------------------------
    // 🔥 OpenGL 3D Viewer 실행 (Lane3D, Object3D)
    // --------------------------------------------
    Lane3D_InitOpenGL(argc, argv);
    Lane3D_StartLoop();

    return 0;
}
