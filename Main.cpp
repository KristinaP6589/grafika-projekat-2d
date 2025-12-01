#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include "Util.h"

// ================== Konstante ==================

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 800;

const int TRACK_SEGMENTS = 400;

// 8 sedišta
const int MAX_SEATS = 8;

const float RAIL_HALF_SPACING = 0.025f;   // pola rastojanja između šina

// brzine / fizika
const float START_ACCEL = 0.75f;   // ubrzanje pri startu
const float TARGET_SPEED = 0.22f;   // bazna brzina na ravnom
const float GRAVITY_ACCEL = 1.0f;   // koliko nagib utiče
const float MIN_SPEED = 0.04f;
const float MAX_SPEED = 0.60f;
const float BRAKE_ACCEL = 0.40f;   // kočenje kad je nekome loše
const float RETURN_SPEED = 0.06f;   // mala brzina ka početku
const double PAUSE_DURATION = 10.0;   // pauza kad je nekome loše (s)

// ================== Pomoćne strukture ==================
struct Vec2 {
    float x, y;
};

struct Passenger {
    bool present;  // da li sedi
    bool beltOn;   // za kasnije
    bool sick;     // za kasnije (tasteri 1-8)
};

// stanje vožnje
enum class RideState {
    BOARDING,        // dodavanje putnika / vezivanje pojaseva
    ACCELERATING,    // ubrzava posle ENTER-a
    RUNNING,         // normalna vožnja
    STOPPING_SICK,   // koči jer je nekome loše
    PAUSED_SICK,     // stoji 10 s
    RETURNING        // vraća se ka početku malom brzinom
};

// ================== Globalni podaci ==================
Vec2 trackPoints[TRACK_SEGMENTS];
Passenger passengers[MAX_SEATS];
Vec2 seatWorldPos[MAX_SEATS];   // gde su sedišta u svetu (za klik)

int passengerCount = 0;
bool spaceWasPressed = false;
bool enterWasPressed = false;
bool leftMouseWasPressed = false;
bool bKeyWasPressed = false;                   // za B (pojasevi)
bool rKeyWasPressed = false;                   // za R (reset)
bool numKeyWasPressed[MAX_SEATS] = { false };  // za 1–8

// parametar kretanja po šini [0,1]
float wagonT = 0.0f;            //pocetak putanje
float wagonSpeed = 0.15f;          //  “brzina” po putanji (t u sekundi)
bool rideRunning = false;          //  da li se vagon trenutno vozi

RideState rideState = RideState::BOARDING;
bool      clearingPassengers = false;  // posle povratka klik skida putnike
double    sickPauseTimer = 0.0;
int       sickPassengerIndex = -1;
bool      returningForward = false;

Vec2 seatOffsets[MAX_SEATS] = {
    {  0.10f, -0.04f },
    {  0.08f,  0.03f },
    {  0.04f, -0.04f },
    {  0.02f,  0.03f },
    { -0.02f, -0.04f },   // donje
    { -0.04f,  0.03f },   // gornje         //svaki sledeci red za 6manje
    { -0.08f, -0.04f },   // donje levo   (za 2 manje od gornjeg)
    { -0.10f,  0.03f },   // gornje levo (x,y)   po 2 u redu
};

// Kontrolne tacke pruge (grubo kao na tvojoj slici)
const int NUM_CTRL = 10;   // broj kontrolnih tačaka
Vec2 ctrlPoints[NUM_CTRL] = {
    // leva strana – start i bregovi
    { -0.90f, -0.30f },   // 0 start
    { -0.65f,  0.10f },   // 1 prvi uspon
    { -0.35f,  0.55f },   // 2 veliki vrh
    { -0.05f,  0.05f },   // 3 dolina
    //drugi veci breg
    {  0.35f,  0.80f },   // 4 veliki vrh
    {  0.60f,  0.05f },   // 5 dolina iza njega
    // treci, uzi breg
    {  0.85f,  0.45f },   // 6 treci vrh
    {  0.95f,  0.00f },   // 7 spuštanje
    // dugačka donja ravnina nazad ka početku
    {  0.50f, -0.35f },   // 8 donja desno
    { -0.40f, -0.35f }    // 9 donja blizu starta (zatvaranje)
};



// Catmull-Rom interpolacija izmedju p0,p1,p2,p3
static Vec2 catmullRom(float t, const Vec2& p0, const Vec2& p1,
    const Vec2& p2, const Vec2& p3)
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

// ================== Reset putnika ==================

void resetPassengers()
{
    passengerCount = 0;
    for (int i = 0; i < MAX_SEATS; ++i) {
        passengers[i].present = false;
        passengers[i].beltOn = false;
        passengers[i].sick = false;
    }
}
void fullReset()
{
    // položaj i brzina
    wagonT = 0.0f;
    wagonSpeed = 0.0f;
    rideRunning = false;          // ako još uvek koristiš ovu promenljivu

    // stanje vožnje
    rideState = RideState::BOARDING;
    clearingPassengers = false;
    sickPauseTimer = 0.0;
    sickPassengerIndex = -1;

    // putnici
    resetPassengers();

    // “edge trigger” promenljive za tastaturu/miš
    spaceWasPressed = false;
    enterWasPressed = false;
    leftMouseWasPressed = false;
    bKeyWasPressed = false;
    rKeyWasPressed = false;
    for (int i = 0; i < MAX_SEATS; ++i)
        numKeyWasPressed[i] = false;
    std::cout << "RESET: sve vraceno na pocetak.\n";
}
void finishReturnToStart()
{
    wagonT = 0.0f;
    wagonSpeed = 0.0f;

    // automatski odveži sve putnike i izleči ih
    for (int i = 0; i < MAX_SEATS; ++i) {
        if (passengers[i].present) {
            passengers[i].beltOn = false;
            passengers[i].sick = false;
        }
    }

    sickPassengerIndex = -1;
    clearingPassengers = true;       // klik sad izbacuje putnike
    rideState = RideState::BOARDING; // opet stanje ukrcavanja
}

// ================== Dodavanje putnika (Space) ==================

void addPassenger()
{
    if (passengerCount >= MAX_SEATS) return; // pun vagon

    for (int i = 0; i < MAX_SEATS; ++i) {
        if (!passengers[i].present) {
            passengers[i].present = true;
            passengers[i].beltOn = false;
            passengers[i].sick = false;
            passengerCount++;
            break;
        }
    }
}
// ================== Toggle pojasa na klik mišem ==================
void toggleSeatBeltClick(float mouseX_ndc, float mouseY_ndc)
{
    // pola širine/visine sedišta (0.05f, 0.04f u drawWagonAndPassengers)
    const float halfW = 0.05f * 0.5f;
    const float halfH = 0.04f * 0.5f;

    for (int i = 0; i < MAX_SEATS; ++i)
    {
        if (!passengers[i].present) continue;   // prazno sedište nas ne zanima

        Vec2 p = seatWorldPos[i];

        if (std::fabs(mouseX_ndc - p.x) <= halfW &&
            std::fabs(mouseY_ndc - p.y) <= halfH)
        {
            if (clearingPassengers) {
                // posle povratka – klik izbacuje putnika
                passengers[i].present = false;
                passengers[i].beltOn = false;
                passengers[i].sick = false;
                passengerCount--;
                if (passengerCount <= 0) {
                    clearingPassengers = false; // sad može nova tura
                }
            }
            else if (rideState == RideState::BOARDING) {
                // normalno stanje – klik kači/otkači pojas
                passengers[i].beltOn = !passengers[i].beltOn;
            }
            break;
        }
    }
}

// ================== Pravljenje putanje (šine) ==================
// prvi deo ravan, posle talasi
void buildTrack()
{
    // PERIODIČNI Catmull–Rom: staza je zatvorena (poslednja tačka = prva)
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        float u = (float)i / (float)(TRACK_SEGMENTS - 1);   // [0,1)

        // koliko segmenata imamo
        float s = u * NUM_CTRL;
        int seg = (int)std::floor(s);
        float t = s - (float)seg;                     // lokalni t u [0,1]

        // wrap-around indeksi (zatvorena staza)
        int i0 = (seg - 1 + NUM_CTRL) % NUM_CTRL;
        int i1 = (seg + 0) % NUM_CTRL;
        int i2 = (seg + 1) % NUM_CTRL;
        int i3 = (seg + 2) % NUM_CTRL;

        trackPoints[i] = catmullRom(t,
            ctrlPoints[i0], ctrlPoints[i1],
            ctrlPoints[i2], ctrlPoints[i3]);
    }

    // eksplicitno zatvorimo
    trackPoints[TRACK_SEGMENTS - 1] = trackPoints[0];
}


// vrati poziciju na šini za zadati parametar t u [0,1]
Vec2 sampleTrack(float t)
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


// ugao šine u tački t – NUMERIČKI iz sampleTrack, da uvek prati pravu putanju
float trackAngle(float t)
{
    const float eps = 1.0f / (float)TRACK_SEGMENTS;

    Vec2 p0 = sampleTrack(t - eps);
    Vec2 p1 = sampleTrack(t + eps);

    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;

    return std::atan2(dy, dx);
}



// ================== Iscrtavanje ==================

// šine
void drawTrack(GLuint shader, GLuint vaoTrack, GLuint vaoQuad)
{
    glUseProgram(shader);

    GLint locPos = glGetUniformLocation(shader, "uPos");
    GLint locScale = glGetUniformLocation(shader, "uScale");
    GLint locColor = glGetUniformLocation(shader, "uColor");
    GLint locMode = glGetUniformLocation(shader, "uMode");
    GLint locAngle = glGetUniformLocation(shader, "uAngle");

    // ne skaliramo, već su verteksi u clip space-u
    glUniform1f(locAngle, 0.0f);
    glUniform2f(locScale, 1.0f, 1.0f);
    glUniform1i(locMode, 1);  // blago osvetljenje

    // ===================== SINE ======================

    glBindVertexArray(vaoTrack);
    glLineWidth(4.0f);

    // leva šina (malo ulevo)
    glUniform3f(locColor, 0.85f, 0.85f, 0.90f);
    glUniform2f(locPos, -RAIL_HALF_SPACING, 0.0f);
    glDrawArrays(GL_LINE_STRIP, 0, TRACK_SEGMENTS);

    // desna šina (malo udesno)
    glUniform2f(locPos, +RAIL_HALF_SPACING, 0.0f);
    glDrawArrays(GL_LINE_STRIP, 0, TRACK_SEGMENTS);         //TREBA DA SE LOOPUJE SINE,    I MOGU POMOCU TACAKA DA SE ISCRTAJU  

    // ===================== PRAGOVI ======================

    glBindVertexArray(vaoQuad);
    glUniform3f(locColor, 0.45f, 0.30f, 0.15f);  // braonkasti pragovi
    glUniform1f(locAngle, 0.0f);   // da ne rotiramo kvadrat pragova

    // svakih ~6% putanje jedan prag
    for (float t = 0.0f; t <= 0.97f; t += 0.06f)
    {
        Vec2 p = sampleTrack(t);

        // malo spusti prag ispod centra šine
        float y = p.y - 0.035f;

        glUniform2f(locPos, p.x, y);
        glUniform2f(locScale, 0.08f, 0.01f);  // sirina, visina

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

// vagon + sedista + ljudi
void drawWagonAndPassengers(GLuint shader, GLuint vaoQuad)
{
    glUseProgram(shader);
    glBindVertexArray(vaoQuad);

    GLint locPos = glGetUniformLocation(shader, "uPos");
    GLint locScale = glGetUniformLocation(shader, "uScale");
    GLint locColor = glGetUniformLocation(shader, "uColor");
    GLint locMode = glGetUniformLocation(shader, "uMode");
    GLint locAngle = glGetUniformLocation(shader, "uAngle");

    glUniform1i(locMode, 1);  // blago osvetljenje na svemu

    // ===================== POZICIJA VAGONA + ugao ======================
    Vec2 p = sampleTrack(wagonT);  // pozicija na sini
    // ===== 2) ugao tangente iz ANALITICKE funkcije =====
    float angle = trackAngle(wagonT);
    // Ugao kojim CRTAMO vagon (može da se razlikuje kad se vraća)
    float drawAngle = angle;
    // ako se vraca i nalazi se dole na donjoj stazi (y dosta nisko),
    // okreni ga za 180 stepeni
    if (rideState == RideState::RETURNING && p.y < -0.25f) {
        drawAngle += (float)M_PI;
    }

    glUniform1f(locAngle, drawAngle);

    Vec2 tangent;
    tangent.x = std::cos(angle);
    tangent.y = std::sin(angle);

    // ===== 3) normalni vektor na šinu =====
    // tangent ugao = angle -> tangent = (cos(angle), sin(angle))
    // normal = tangent rotiran za +90°: (-sin, cos)
    Vec2 normal;
    normal.x = -std::sin(angle);
    normal.y = std::cos(angle);

    // ako normal gleda nadole, okreni je – vagon će uvek biti "gore"
    if (normal.y < 0.0f) {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }

    // ===== 4) centar vagona malo iznad šine duž normale =====
    float distFromRails = 0.08f;   // probaj 0.07 / 0.09 ako treba
    Vec2 center;
    center.x = p.x + normal.x * distFromRails;
    center.y = p.y + normal.y * distFromRails;

    // ===================== TELA VAGONA ======================
    // glavno telo (crveno)
    glUniform3f(locColor, 0.85f, 0.15f, 0.15f);     //boja
    glUniform2f(locPos, center.x, center.y);            //pozicija
    glUniform2f(locScale, 0.28f, 0.12f);            //velicina - sirina, visina
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // ===================== SEDIŠTA ======================
    float cosA = std::cos(drawAngle);
    float sinA = std::sin(drawAngle);

    glUniform3f(locColor, 0.65f, 0.65f, 0.70f);

    for (int i = 0; i < MAX_SEATS; ++i)
    {
        // Rotiraj lokalni offset sedišta
        float localX = seatOffsets[i].x;
        float localY = seatOffsets[i].y;

        float rotatedX = localX * cosA - localY * sinA;
        float rotatedY = localX * sinA + localY * cosA;

        Vec2 seatPos;
        seatPos.x = center.x + rotatedX;
        seatPos.y = center.y + rotatedY;

        seatWorldPos[i] = seatPos;    // zapamti za klik mišem

        glUniform2f(locPos, seatPos.x, seatPos.y);
        glUniform2f(locScale, 0.05f, 0.04f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // ===================== PUTNICI ======================
    for (int i = 0; i < MAX_SEATS; ++i)
    {
        if (!passengers[i].present) continue;

        // Rotiraj lokalni offset sedišta
        float localX = seatOffsets[i].x;
        float localY = seatOffsets[i].y;

        float rotatedX = localX * cosA - localY * sinA;
        float rotatedY = localX * sinA + localY * cosA;

        Vec2 seatPos;
        seatPos.x = center.x + rotatedX;
        seatPos.y = center.y + rotatedY;

        seatPos = seatWorldPos[i];     //zapamti poziciju za pojas

        // **TELO PUTNIKA - offset od sedišta ROTIRAN**
        Vec2 teloOffset = { 0.0f, 0.05f };
        float teloRotX = teloOffset.x * cosA - teloOffset.y * sinA;
        float teloRotY = teloOffset.x * sinA + teloOffset.y * cosA;

        // boja tela: plavo normalno, zeleno ako je sick
        if (passengers[i].sick)
            glUniform3f(locColor, 0.10f, 0.70f, 0.20f);   // "muka" – zelenkast
        else
            glUniform3f(locColor, 0.12f, 0.30f, 0.95f);   // normalno plavo

        glUniform2f(locPos, seatPos.x + teloRotX, seatPos.y + teloRotY);
        glUniform2f(locScale, 0.03f, 0.07f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // **GLAVA PUTNIKA - offset od sedišta ROTIRAN**
        Vec2 glavaOffset = { 0.0f, 0.11f };
        float glavaRotX = glavaOffset.x * cosA - glavaOffset.y * sinA;
        float glavaRotY = glavaOffset.x * sinA + glavaOffset.y * cosA;

        // boja glave: bela normalno, zeleno ako je sick
        if (passengers[i].sick)
            glUniform3f(locColor, 0.10f, 0.70f, 0.20f);   // "muka" – zelenkast
        else
            glUniform3f(locColor, 0.98f, 0.90f, 0.75f);   // normalno belo

        //glUniform3f(locColor, 0.98f, 0.90f, 0.75f);
        glUniform2f(locPos, seatPos.x + glavaRotX, seatPos.y + glavaRotY);
        glUniform2f(locScale, 0.03f, 0.03f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // ================== POJAS (ako je vezan) ==================
        if (passengers[i].beltOn)
        {
            // pojas ide preko stomaka, malo ispod tela
            Vec2 pojasOffset = { 0.0f, 0.02f };
            float pojasRotX = pojasOffset.x * cosA - pojasOffset.y * sinA;
            float pojasRotY = pojasOffset.x * sinA + pojasOffset.y * cosA;

            glUniform3f(locColor, 0.90f, 0.85f, 0.10f);  // žut/ zlatan pojas
            glUniform2f(locPos, seatPos.x + pojasRotX, seatPos.y + pojasRotY);
            glUniform2f(locScale, 0.04f, 0.01f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
}


void drawBackground(GLuint shader, GLuint vaoQuad)
{
    glUseProgram(shader);
    glBindVertexArray(vaoQuad);

    GLint locAngle = glGetUniformLocation(shader, "uAngle"); 
    glUniform1f(locAngle, 0.0f);

    // kvadrat [-0.5, 0.5] skaliramo na ceo ekran
    glUniform2f(glGetUniformLocation(shader, "uPos"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(shader, "uScale"), 2.0f, 2.0f);

    // uColor se ovde ne koristi (uMode = 2), ali moramo da ga setujemo
    glUniform3f(glGetUniformLocation(shader, "uColor"), 0.0f, 0.0f, 0.0f);

    glUniform1i(glGetUniformLocation(shader, "uMode"), 2);
    glUniform3f(glGetUniformLocation(shader, "uSkyTop"), 0.75f, 0.85f, 1.0f);
    glUniform3f(glGetUniformLocation(shader, "uSkyBottom"), 0.35f, 0.45f, 0.90f);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// ================== MAIN ==================

int endProgram(const std::string& msg)
{
    std::cout << msg << std::endl;
    glfwTerminate();
    return -1;
}

int main()
{
    // GLFW inicijalizacija
    if (!glfwInit()) return endProgram("GLFW init failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rollercoaster", nullptr, nullptr);
    if (!window) return endProgram("Prozor nije uspeo da se kreira.");

    glfwMakeContextCurrent(window);

    // GLEW
    if (glewInit() != GLEW_OK) return endProgram("GLEW init failed.");

    // Šejder
    GLuint basicShader = createShader("basic.vert", "basic.frag");
    if (!basicShader) return endProgram("Neuspeh pri kreiranju sejdera.");
    glLineWidth(4.0f);   // šine deblje

    // Kursor šine (ako postoji rails.png)
    GLFWcursor* railsCursor = loadImageToCursor("res/rails.png");
    if (railsCursor) {
        glfwSetCursor(window, railsCursor);
    }
    else {
        std::cout << "Kursor nije ucitan! Putanja: res/rails.png\n";
    }

    // Pravimo putanju
    buildTrack();

    // ============== VAO za šine ==============
    
    GLuint vaoTrack, vboTrack;
    glGenVertexArrays(1, &vaoTrack);
    glGenBuffers(1, &vboTrack);

    glBindVertexArray(vaoTrack);
    glBindBuffer(GL_ARRAY_BUFFER, vboTrack);
    glBufferData(GL_ARRAY_BUFFER, sizeof(trackPoints), trackPoints, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void*)0);
    glEnableVertexAttribArray(0);

    // ============== VAO za kvadrat (vagon, sedišta, putnici) ==============

    float quadVerts[] = {
        -0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f
    };

    GLuint vaoQuad, vboQuad;
    glGenVertexArrays(1, &vaoQuad);
    glGenBuffers(1, &vboQuad);

    glBindVertexArray(vaoQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // početna stanja
    fullReset();

    glClearColor(0.4f, 0.5f, 0.95f, 1.0f);

    //  vreme za dt
    double lastTime = glfwGetTime();

    // ======================== GLAVNA PETLJA ========================

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();        
        double dt = now - lastTime;        
        lastTime = now;                    

        // --- input: samo SPACE za sada ---
        int spaceState = glfwGetKey(window, GLFW_KEY_SPACE);
        if (spaceState == GLFW_PRESS && !spaceWasPressed) {
            if (rideState == RideState::BOARDING && !clearingPassengers) {
                addPassenger();
            }
        }
        spaceWasPressed = (spaceState == GLFW_PRESS);

        // --- input: ENTER start/stop voznje ---
        int enterState = glfwGetKey(window, GLFW_KEY_ENTER);
        if (enterState == GLFW_PRESS && !enterWasPressed) {
            if (rideState == RideState::BOARDING && !clearingPassengers) {
                // pokusavamo da KRENEMO → proveri pojaseve
                bool allSafe = true;
                for (int i = 0; i < MAX_SEATS; ++i) {
                    if (passengers[i].present && !passengers[i].beltOn) {
                        allSafe = false;
                        break;
                    }
                }
                if (allSafe && passengerCount > 0) {
                    rideState = RideState::ACCELERATING;
                    wagonSpeed = 0.0f;
                }
                else {
                    std::cout << "Neko nema vezan pojas ili nema putnika – voznja ne krece.\n";
                }
            }
            else {
                // voznja vec ide → ENTER je samo stop
                rideRunning = false;
            }
        }
        enterWasPressed = (enterState == GLFW_PRESS);

        // --- input: LEVI KLIK – pojas ---
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        int leftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (leftState == GLFW_PRESS && !leftMouseWasPressed) {
            // ekran → NDC [-1,1]
            float xNdc = (float(mx) / (float)SCREEN_WIDTH) * 2.0f - 1.0f;
            float yNdc = 1.0f - (float(my) / (float)SCREEN_HEIGHT) * 2.0f;
            toggleSeatBeltClick(xNdc, yNdc);
        }
        leftMouseWasPressed = (leftState == GLFW_PRESS);


        // --- tasteri 1–8: nekome je lose ---
        if (rideState == RideState::ACCELERATING || rideState == RideState::RUNNING) {
            for (int k = 0; k < MAX_SEATS; ++k) {
                int key = GLFW_KEY_1 + k;
                int st = glfwGetKey(window, key);
                if (st == GLFW_PRESS) {
                    if (passengers[k].present) {
                        passengers[k].sick = true;
                        sickPassengerIndex = k;
                        rideState = RideState::STOPPING_SICK;
                    }
                    break;
                }
            }
        }

        // --- B: vezivanje / skidanje pojaseva za sve prisutne ---
        int bState = glfwGetKey(window, GLFW_KEY_B);
        if (bState == GLFW_PRESS && !bKeyWasPressed)
        {
            bool biloVezano = false;
            for (int i = 0; i < MAX_SEATS; ++i)
                if (passengers[i].present && passengers[i].beltOn)
                    biloVezano = true;

            // ako je bar jedan bio vezan -> skini sve,
            // inače veži sve
            bool newState = !biloVezano;
            for (int i = 0; i < MAX_SEATS; ++i)
                if (passengers[i].present)
                    passengers[i].beltOn = newState;
        }
        bKeyWasPressed = (bState == GLFW_PRESS);

        // --- R: totalni reset cele vožnje ---
        int rState = glfwGetKey(window, GLFW_KEY_R);
        if (rState == GLFW_PRESS && !rKeyWasPressed)
        {
            fullReset();
        }
        rKeyWasPressed = (rState == GLFW_PRESS);

        // --- kretanje vagona po sinama ---    
        if (rideRunning) {
            wagonT += wagonSpeed * (float)dt;
            // ako predje kraj putanje, vracamo na pocetak (vozi u krug)
            if (wagonT > 1.0f) wagonT -= 1.0f;
            if (wagonT < 0.0f) wagonT += 1.0f;
        }


        // --- fizika vožnje / brzina ---
        if (rideState == RideState::ACCELERATING ||
            rideState == RideState::RUNNING ||
            rideState == RideState::STOPPING_SICK ||
            rideState == RideState::RETURNING)
        {
            float angle = trackAngle(wagonT);
            float slopeY = std::sin(angle);

            switch (rideState)
            {
            case RideState::ACCELERATING:
                wagonSpeed += START_ACCEL * (float)dt;
                if (wagonSpeed > TARGET_SPEED) wagonSpeed = TARGET_SPEED;

                wagonT += wagonSpeed * (float)dt;
                if (wagonT > 1.0f) wagonT -= 1.0f;

                if (wagonSpeed >= TARGET_SPEED * 0.999f)
                    rideState = RideState::RUNNING;
                break;

            case RideState::RUNNING:
            {
                // nagib – sin ugla; >0 = uzbrdo, <0 = nizbrdo (za naš smer putanje)
                float slope = slopeY;

                // koliko jako guramo nizbrdo / kočimo uzbrdo
                const float DOWNHILL_ACCEL = 3.0f;   // povećaj ako hoćeš još ludje
                const float UPHILL_BRAKE = 3.5f;   // povećaj da bude još sporiji uzbrdo

                if (slope > 0.0f) {
                    // UZBRDO – jako usporavanje
                    wagonSpeed -= UPHILL_BRAKE * slope * (float)dt;
                }
                else {
                    // NIZBRDO – jako ubrzavanje
                    wagonSpeed += DOWNHILL_ACCEL * (-slope) * (float)dt;
                }

                // Na skoro ravnim delovima blago vučemo brzinu ka TARGET_SPEED
                float steepness = std::fabs(slope);                      // 0 = ravno, 1 = strmo
                float flatness = 1.0f - std::min(1.0f, steepness * 4);  // <~0.25 = ravno
                const float FRICTION = 1.0f;
                wagonSpeed += (TARGET_SPEED - wagonSpeed) * flatness * FRICTION * (float)dt;

                // ograničenja
                if (wagonSpeed < MIN_SPEED) wagonSpeed = MIN_SPEED;
                if (wagonSpeed > MAX_SPEED) wagonSpeed = MAX_SPEED;

                // pomeri vagon po putanji
                wagonT += wagonSpeed * (float)dt;
                if (wagonT > 1.0f) wagonT -= 1.0f;
                break;
            }
            case RideState::STOPPING_SICK:
                wagonSpeed -= BRAKE_ACCEL * (float)dt;
                if (wagonSpeed <= 0.0f) {
                    wagonSpeed = 0.0f;
                    rideState = RideState::PAUSED_SICK;
                    sickPauseTimer = 0.0;
                }
                else {
                    wagonT += wagonSpeed * (float)dt;
                    if (wagonT > 1.0f) wagonT -= 1.0f;
                }
                break;

            case RideState::RETURNING:

                if (returningForward) {
                    // idemo napred ka t = 1.0 pa wrap na 0
                    wagonT += RETURN_SPEED * (float)dt;

                    if (wagonT >= 1.0f) {
                        finishReturnToStart();   // postavi t=0 i odveži sve
                    }
                }
                else {
                    // idemo unazad ka t = 0.0
                    wagonT -= RETURN_SPEED * (float)dt;

                    if (wagonT <= 0.0f) {
                        finishReturnToStart();
                    }
                }
                break;

            default:
                break;
            }
        }
        if (rideState == RideState::PAUSED_SICK) {
            sickPauseTimer += dt;
            if (sickPauseTimer >= PAUSE_DURATION) {
                // izaberi smer koji je kraci do pocetka
                float distBack = wagonT;           // od t do 0 unazad
                float distFwd = 1.0f - wagonT;    // od t do 1 unapred (pa wrap na 0)

                returningForward = (distFwd < distBack);  // true = idemo napred ka 1

                rideState = RideState::RETURNING;
            }
        }


        // --- crtanje ---

        glClear(GL_COLOR_BUFFER_BIT);

        drawBackground(basicShader, vaoQuad);
    
        drawTrack(basicShader, vaoTrack, vaoQuad);
        drawWagonAndPassengers(basicShader, vaoQuad);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
