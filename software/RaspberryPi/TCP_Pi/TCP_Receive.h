#pragma once

#include <stdint.h>
#include <pthread.h>
#include "TCP_Protocol.h"     // Lane/Object 구조체 정의 포함
#include "TCP_Buffer.h"       // 공유 버퍼 사용하려면 필요


void* TCP_ReceiveThread(void* arg);
int recvAll(int sock, uint8_t* buf, int size);