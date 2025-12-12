#include "TCP_Buffer.h"
#include <string.h>  // memcpy


uint8_t TCP_Lepton_Buffer[120][160][3];
pthread_mutex_t TCP_Lepton_mtx = PTHREAD_MUTEX_INITIALIZER;
bool new_Lepton_flag = false;

/* ============================================================ */

void TCP_Lepton_Buffer_Push(uint8_t Normalized_Frame[120][160][3])
{
    pthread_mutex_lock(&TCP_Lepton_mtx); 

    memcpy(TCP_Lepton_Buffer, Normalized_Frame, sizeof(TCP_Lepton_Buffer));

    new_Lepton_flag = true;

    pthread_mutex_unlock(&TCP_Lepton_mtx);
}



bool TCP_Lepton_Buffer_Pop(uint8_t Normalized_Frame[120][160][3])
{
    pthread_mutex_lock(&TCP_Lepton_mtx);

    if (!new_Lepton_flag) 
    {
        pthread_mutex_unlock(&TCP_Lepton_mtx);
        return false;
    }

    memcpy(Normalized_Frame, TCP_Lepton_Buffer, sizeof(TCP_Lepton_Buffer));
    new_Lepton_flag = false;

    pthread_mutex_unlock(&TCP_Lepton_mtx);
    return true;
}

/* ============================================================ */

uint8_t TCP_RGB_Buffer[640][640][3];
pthread_mutex_t TCP_RGB_mtx = PTHREAD_MUTEX_INITIALIZER;
bool new_rgb_flag = false;

/* ============================================================ */

void TCP_RGB_Buffer_Push(uint8_t* RGB_Frame)
{
    pthread_mutex_lock(&TCP_RGB_mtx);

    memcpy(TCP_RGB_Buffer, RGB_Frame, sizeof(TCP_RGB_Buffer));

    new_rgb_flag = true;

    pthread_mutex_unlock(&TCP_RGB_mtx);
}



bool TCP_RGB_Buffer_Pop(uint8_t RGB_Frame[640][640][3])
{
    pthread_mutex_lock(&TCP_RGB_mtx);

    if (!new_rgb_flag)
    {
        pthread_mutex_unlock(&TCP_RGB_mtx);
        return false;
    }

    memcpy(RGB_Frame, TCP_RGB_Buffer, sizeof(TCP_RGB_Buffer));
    new_rgb_flag = false;

    pthread_mutex_unlock(&TCP_RGB_mtx);
    return true;
}

/* ============================================================ */

std::vector<Lane> TCP_Lane_Buffer;
pthread_mutex_t TCP_Lane_mtx = PTHREAD_MUTEX_INITIALIZER;
bool new_lane_flag = false;

/* ============================================================ */

void TCP_LANE_Buffer_Push(const std::vector<Lane>& laneData)
{
    pthread_mutex_lock(&TCP_Lane_mtx);

    TCP_Lane_Buffer = laneData;   // 최신 데이터로 교체
    new_lane_flag = true;

    pthread_mutex_unlock(&TCP_Lane_mtx);
}

bool TCP_LANE_Buffer_Pop(std::vector<Lane>& outLane)
{
    pthread_mutex_lock(&TCP_Lane_mtx);

    if (!new_lane_flag)
    {
        pthread_mutex_unlock(&TCP_Lane_mtx);
        return false;
    }

    outLane = TCP_Lane_Buffer;   // 최신 데이터 전달
    new_lane_flag = false;

    pthread_mutex_unlock(&TCP_Lane_mtx);
    return true;
}

/* ============================================================ */

std::vector<Object> TCP_Object_Buffer;
pthread_mutex_t TCP_Object_mtx = PTHREAD_MUTEX_INITIALIZER;
bool new_object_flag = false;

/* ============================================================ */

void TCP_Object_Buffer_Push(const std::vector<Object>& objData)
{
    pthread_mutex_lock(&TCP_Object_mtx);

    TCP_Object_Buffer = objData;   // 최신 데이터로 갱신
    new_object_flag = true;

    pthread_mutex_unlock(&TCP_Object_mtx);
}

bool TCP_Object_Buffer_Pop(std::vector<Object>& outObj)
{
    pthread_mutex_lock(&TCP_Object_mtx);

    if (!new_object_flag)
    {
        pthread_mutex_unlock(&TCP_Object_mtx);
        return false;
    }

    outObj = TCP_Object_Buffer;     // 최신 데이터 전달
    new_object_flag = false;

    pthread_mutex_unlock(&TCP_Object_mtx);
    return true;
}

/* ============================================================ */