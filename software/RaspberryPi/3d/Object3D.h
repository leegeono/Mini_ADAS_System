#pragma once

void Object3D_Init();

// 내 차량(Ego)
void Object3D_SetEgo(float x, float z, float scale);
void Object3D_DrawEgo();

// 외부 차량 (YOLO 검출된 자동차)
void Object3D_DrawCar(float x, float z, float scale);

// 필요하면 사람도
void Object3D_DrawPerson(float x, float z, float scale);
