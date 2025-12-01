#version 330 core

layout(location = 0) in vec2 inPos;

uniform vec2 uPos;     // translacija
uniform vec2 uScale;   // skaliranje
uniform vec3 uColor;   // osnovna boja objekta
uniform float uAngle;   //  ugao rotacije u radijanima

out vec3 vColor;
out vec2 vWorldPos;    // pozicija u clip-space za osvetljenje / nebo

void main()
{
    // 1) lokalno skaliranje kvadrata
    vec2 pos   = inPos * uScale;
    
    // 2) rotacija oko (0,0) za uAngle
    float c = cos(uAngle);
    float s = sin(uAngle);
    mat2 R = mat2(c, s, -s,  c);
    pos = R * pos;

    // pomeranje na poziciju
    pos += uPos;
    
    vWorldPos = pos;
    vColor    = uColor;

    gl_Position = vec4(pos, 0.0, 1.0);
}
