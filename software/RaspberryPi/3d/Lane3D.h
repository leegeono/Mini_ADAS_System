#pragma once
#include <vector>
#include "../TCP_Pi/TCP_Buffer.h"   // Lane 구조체 있는 곳 포함

void Lane3D_InitOpenGL(int argc, char** argv);
void Lane3D_SetLane(const std::vector<Lane>& lanes);
void Lane3D_StartLoop();

