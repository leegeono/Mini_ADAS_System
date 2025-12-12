#include "TCP_Receive.h"
#include "TCP_Buffer.h"
#include "TCP_Protocol.h"
#include <cstdio>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>

// ============================================================
// recvAll: size만큼 정확히 받아오기
// ============================================================
int recvAll(int sock, uint8_t* buf, int size)
{
    int total = 0;
    while (total < size)
    {
        int ret = recv(sock, buf + total, size - total, 0);
        if (ret <= 0)
            return ret;
        total += ret;
    }
    return total;
}

// ============================================================
// TCP 수신 스레드
// ============================================================
void* TCP_ReceiveThread(void* arg)
{
    int sock = *(int*)arg;

    while (1)
    {
        // ------------------------------------
        // 1) HEADER (8 bytes) 수신
        // ------------------------------------
        uint8_t headerBuf[8];

        int ret = recvAll(sock, headerBuf, 8);
        if (ret <= 0)
        {
            printf("[RECV] Disconnected\n");
            break;
        }

        uint32_t packet_type = *(uint32_t*)&headerBuf[0];
        uint32_t count       = *(uint32_t*)&headerBuf[4];

        // ------------------------------------
        // 2) payload 크기 계산
        // ------------------------------------
        int payloadSize = 0;

        if (packet_type == LANE)
            payloadSize = count * 4;      // (x,y) = 4 bytes
        else if (packet_type == OBJ)
            payloadSize = count * 12;     // 6 * 2 bytes
        else
            continue;                      // 잘못된 타입 무시

        // ------------------------------------
        // payload가 0이면 바로 Push 없이 continue
        // ------------------------------------
        if (payloadSize == 0)
        {
            if (packet_type == LANE)
            {
                std::vector<Lane> empty;
                TCP_LANE_Buffer_Push(empty);
            }
            else if (packet_type == OBJ)
            {
                std::vector<Object> empty;
                TCP_Object_Buffer_Push(empty);
            }
            continue;
        }

        // ------------------------------------
        // 3) payload 수신
        // ------------------------------------
        uint8_t* payload = (uint8_t*)malloc(payloadSize);
        if (!payload)
        {
            printf("[RECV] malloc failed\n");
            break;
        }

        ret = recvAll(sock, payload, payloadSize);
        if (ret <= 0)
        {
            free(payload);
            printf("[RECV] payload recv failed\n");
            break;
        }

        // ------------------------------------
        // 4) payload 파싱 후 버퍼로 push
        // ------------------------------------
        if (packet_type == LANE)
        {
            std::vector<Lane> lanes;
            lanes.reserve(count);

            for (int i = 0; i < count; i++)
            {
                int off = i * 4;
                Lane ln;
                ln.x = *(uint16_t*)(payload + off);
                ln.y = *(uint16_t*)(payload + off + 2);
                lanes.push_back(ln);
            }

            TCP_LANE_Buffer_Push(lanes);
        }
        else if (packet_type == OBJ)
        {
            std::vector<Object> objs;
            objs.reserve(count);

           for (int i = 0; i < count; i++)
            {
            int off = i * 12;

            Object o;

            o.cls  = (uint16_t)(payload[off] | (payload[off+1] << 8));
            o.conf = (uint16_t)(payload[off+2] | (payload[off+3] << 8));

            o.x1   = (uint16_t)(payload[off+4] | (payload[off+5] << 8));
            o.y1   = (uint16_t)(payload[off+6] | (payload[off+7] << 8));
            o.x2   = (uint16_t)(payload[off+8] | (payload[off+9] << 8));
            o.y2   = (uint16_t)(payload[off+10] | (payload[off+11] << 8));

            objs.push_back(o);
            }

            TCP_Object_Buffer_Push(objs);
        }

        free(payload);
    }

    close(sock);
    pthread_exit(NULL);
}
