#pragma once 

#include <stdint.h>
#include <pthread.h>
#include "TCP_Protocol.h"
#include <vector>

/* =============== Lepton 의 한 프레임을 받아오기  ================ */
extern uint8_t TCP_Lepton_Buffer[120][160][3]; 
extern pthread_mutex_t TCP_Lepton_mtx;
extern bool new_Lepton_flag;

void TCP_Lepton_Buffer_Push(uint8_t Normalized_Frame[120][160][3]);
bool TCP_Lepton_Buffer_Pop(uint8_t Normalized_Frame[120][160][3]);

/* ============================================================ */


/* =============== RGB 의 한 프레임을 받아오기  ================ */
extern uint8_t TCP_RGB_Buffer[640][640][3];
extern pthread_mutex_t TCP_RGB_mtx;
extern bool new_rgb_flag;

void TCP_RGB_Buffer_Push(uint8_t* RGB_Frame);
bool TCP_RGB_Buffer_Pop(uint8_t RGB_Frame[640][640][3]);

/* ============================================================ */


/* =============== 차선 좌표 받아와서 저장하는 버퍼 ================ */
extern std::vector<Lane> TCP_Lane_Buffer;
extern pthread_mutex_t TCP_Lane_mtx;
extern bool new_lane_flag;

void TCP_LANE_Buffer_Push(const std::vector<Lane>& laneData);
bool TCP_LANE_Buffer_Pop(std::vector<Lane>& outLane);

/* ============================================================ */


/* =============== 객체 좌표 받아와서 저장하는 버퍼 ================ */
extern std::vector<Object> TCP_Object_Buffer;
extern pthread_mutex_t TCP_Object_mtx;
extern bool new_object_flag;

void TCP_Object_Buffer_Push(const std::vector<Object>& objData);
bool TCP_Object_Buffer_Pop(std::vector<Object>& outObj);
/* ============================================================ */

