#pragma once
#include <stdint.h>

#define LEPTON_FRAME   1
#define RGB_FRAME      2
#define LANE   3
#define OBJ 4

#define ADAS_HEADER_SIZE 8

#define LANE_BYTE_SIZE   (sizeof(Lane))    
#define OBJECT_BYTE_SIZE (sizeof(Object))  

typedef struct
{
    uint32_t type;       
    uint32_t width;      
    uint32_t height;     
    uint32_t channels;   
    uint32_t dataSize;  
} FrameHeader;


typedef struct __attribute__((packed))
{
    uint16_t x;
    uint16_t y;
} Lane;


typedef struct __attribute__((packed))
{
    uint16_t cls;
    uint16_t conf;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} Object;
