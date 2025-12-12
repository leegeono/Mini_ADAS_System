#pragma once
#include <vector>
#include <GL/gl.h>

struct ObjectModel {
    std::vector<float> vertices;  // x,y,z 반복
    std::vector<float> normals;   // nx,ny,nz 반복
    std::vector<unsigned int> indices; // glDrawElements용

    GLuint displayListId = 0;     // 렌더링 가속용

    void BuildDisplayList();      // 로더가 호출
};
