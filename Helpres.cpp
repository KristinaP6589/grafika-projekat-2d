#include "Helpers.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

// Catmull-Rom interpolacija izmedju p0,p1,p2,p3  - glatka kriva za tacke za prugu
static Vec2 catmullRom(float t, const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
    float t2 = t * t;
    float t3 = t2 * t;

    Vec2 r;
    r.x = 0.5f * ((2.0f * p1.x) +
        (-p0.x + p2.x) * t +
        (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
        (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);

    r.y = 0.5f * ((2.0f * p1.y) +
        (-p0.y + p2.y) * t +
        (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
        (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    return r;
}


// ================== Pravljenje putanje (sine) ==================
void buildTrack(Vec2* trackPoints, int TRACK_SEGMENTS, const Vec2* ctrlPoints, int NUM_CTRL)    // prvi deo ravan, posle talasi
{
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {        //  staza je zatvorena (poslednja tacka = prva)
        float u = (float)i / (float)(TRACK_SEGMENTS - 1);   // ide od [0,1)

        // koliko segmenata imamo
        float s = u * NUM_CTRL;
        int seg = (int)std::floor(s);
        float t = s - (float)seg;

        // wrap-around indeksi (zatvorena staza)
        int i0 = (seg - 1 + NUM_CTRL) % NUM_CTRL;
        int i1 = (seg + 0) % NUM_CTRL;
        int i2 = (seg + 1) % NUM_CTRL;
        int i3 = (seg + 2) % NUM_CTRL;

        trackPoints[i] = catmullRom(t,
            ctrlPoints[i0], ctrlPoints[i1],
            ctrlPoints[i2], ctrlPoints[i3]);
    }

    trackPoints[TRACK_SEGMENTS - 1] = trackPoints[0];   //zatvaranje staze
}


// vrati poziciju na sini za zadati parametar t u [0,1]
Vec2 sampleTrack(float t, const Vec2* trackPoints, int TRACK_SEGMENTS)
{
    // t u [0,1), ali dozvoljava i >1 / <0 (vrti u krug)
    if (t <= 0.0f) return trackPoints[0];
    if (t >= 1.0f) return trackPoints[TRACK_SEGMENTS - 1];

    float fIndex = t * (float)(TRACK_SEGMENTS - 1);
    int   i0 = (int)std::floor(fIndex);
    int   i1 = i0 + 1;
    float alpha = fIndex - (float)i0;

    Vec2 p0 = trackPoints[i0];
    Vec2 p1 = trackPoints[i1];

    Vec2 p;
    p.x = p0.x + (p1.x - p0.x) * alpha;
    p.y = p0.y + (p1.y - p0.y) * alpha;
    return p;
}


// ugao sine u tacki t – NUMERICKI iz sampleTrack, da uvek prati pravu putanju
float trackAngle(float t, const Vec2* trackPoints, int TRACK_SEGMENTS)
{
    const float eps = 1.0f / (float)TRACK_SEGMENTS;

    Vec2 p0 = sampleTrack(t - eps, trackPoints, TRACK_SEGMENTS);
    Vec2 p1 = sampleTrack(t + eps, trackPoints, TRACK_SEGMENTS);

    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;

    return std::atan2(dy, dx);
}