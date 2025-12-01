#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct Vec2 {
    float x, y;
};

static Vec2 catmullRom(float t, const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3);

// Pravi celu putanju
void buildTrack(Vec2* trackPoints, int trackSegments, const Vec2* ctrlPoints, int numCtrl);

// Uzorak jedne tacke sa putanje
Vec2 sampleTrack(float t, const Vec2* trackPoints, int trackSegments);

// Ugao tangente na putanji
float trackAngle(float t, const Vec2* trackPoints, int trackSegments);