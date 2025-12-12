#include <GL/glut.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include "Object3D.h"

// ----------------------------------
// Data Structures
// ----------------------------------
struct Vertex { float x, y, z; };

struct Material { float r, g, b; };

struct Face {
    int v1, v2, v3;
    std::string mtl;
};

// ----------------------------------
// Global Buffers (Car / Person 독립)
// ----------------------------------
static std::vector<Vertex> car_vertices;
static std::vector<Face>   car_faces;
static std::map<std::string, Material> car_materials;
static bool carLoaded = false;

static std::vector<Vertex> person_vertices;
static std::vector<Face>   person_faces;
static std::map<std::string, Material> person_materials;
static bool personLoaded = false;

// Ego Transform
static float g_egoX = 0.0f;
static float g_egoZ = 0.0f;
static float g_egoScale = 1.0f;


// ======================================================
// Load MTL
// ======================================================
bool LoadMTL(const std::string& path, std::map<std::string, Material>& mats)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[ERR] Cannot open MTL: " << path << "\n";
        return false;
    }

    std::string line, cur;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string head;
        ss >> head;

        if (head == "newmtl")
        {
            ss >> cur;
        }
        else if (head == "Kd")
        {
            Material m;
            ss >> m.r >> m.g >> m.b;
            mats[cur] = m;
        }
    }
    return true;
}


// ======================================================
// Load OBJ (generic)
// ======================================================
bool LoadOBJ_Color(
    const std::string& path,
    std::vector<Vertex>& verts,
    std::vector<Face>& faces,
    std::map<std::string, Material>& mats,
    bool& loadedFlag)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[ERR] Cannot open OBJ: " << path << "\n";
        return false;
    }

    verts.clear();
    faces.clear();

    std::string line;
    std::string curMtl = "default";

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string head;
        ss >> head;

        if (head == "mtllib")
        {
            std::string mtlfile;
            ss >> mtlfile;
            LoadMTL("./3d/Objects/" + mtlfile, mats);
        }
        else if (head == "usemtl")
        {
            ss >> curMtl;
        }
        else if (head == "v")
        {
            Vertex v;
            ss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
        else if (head == "f")
        {
            Face f;
            ss >> f.v1 >> f.v2 >> f.v3;
            f.v1--; f.v2--; f.v3--;
            f.mtl = curMtl;
            faces.push_back(f);
        }
    }

    std::cout << "[OK] OBJ Loaded: "
              << verts.size() << " vertices, "
              << faces.size()    << " faces\n";

    loadedFlag = true;
    return true;
}


// ======================================================
// Draw Functions (Car / Person 분리)
// ======================================================
static void DrawModel(
    const std::vector<Vertex>& verts,
    const std::vector<Face>& faces,
    const std::map<std::string, Material>& mats)
{
    glBegin(GL_TRIANGLES);

    for (auto& f : faces)
    {
        Material col = mats.at(f.mtl);
        glColor3f(col.r, col.g, col.b);

        const Vertex& a = verts[f.v1];
        const Vertex& b = verts[f.v2];
        const Vertex& c = verts[f.v3];

        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
        glVertex3f(c.x, c.y, c.z);
    }

    glEnd();
}


// ======================================================
// Init: Load Both Models
// ======================================================
void Object3D_Init()
{
    LoadOBJ_Color("./3d/Objects/car.obj",
                  car_vertices, car_faces, car_materials, carLoaded);

    LoadOBJ_Color("./3d/Objects/person.obj",
                  person_vertices, person_faces, person_materials, personLoaded);
}


// ======================================================
// Ego Car
// ======================================================
void Object3D_SetEgo(float x, float z, float scale)
{
    g_egoX = x;
    g_egoZ = z;
    g_egoScale = scale;
}

void Object3D_DrawEgo()
{
    if (!carLoaded) return;

    glPushMatrix();
    glTranslatef(g_egoX, 0.0f, g_egoZ + 7.0f);

    glScalef(g_egoScale * 0.15f,
             g_egoScale * 0.15f,
             g_egoScale * 0.15f);

    glRotatef(-90, 1, 0, 0);
    DrawModel(car_vertices, car_faces, car_materials);

    glPopMatrix();
}


// ======================================================
// Draw Other Cars
// ======================================================
void Object3D_DrawCar(float x, float z, float scale)
{
    if (!carLoaded) return;

    glPushMatrix();

    glTranslatef(x, 0.0f, z);
    glScalef(scale * 0.15f, scale * 0.15f, scale * 0.15f);
    glRotatef(-90, 1, 0, 0);

    DrawModel(car_vertices, car_faces, car_materials);

    glPopMatrix();
}


// ======================================================
// Draw Person
// ======================================================
void Object3D_DrawPerson(float x, float z, float scale)
{
    if (!personLoaded) {
        std::cout << "[WARN] person model not loaded!\n";
        return;
    }

    glPushMatrix();

    // 사람도 자동차처럼 매핑 적용 (x,z 그대로 사용)
    glTranslatef(x, 0.0f, z);

    // 사람 크기만 따로 조절
    glScalef(scale * 0.2f, scale * 0.2f, scale * 0.2f);

    // 회전 동일
    glRotatef(-90, 1, 0, 0);
    glRotatef(90, 0, 0, 1);

    DrawModel(person_vertices, person_faces, person_materials);

    glPopMatrix();
}