#version 330 core

in vec3 vColor;
in vec2 vWorldPos;

out vec4 outColor;

// uMode:
// 0 = ravna boja (svejedno od visine)
// 1 = malo “osvetljenje” – tamnije dole, svetlije gore
// 2 = nebo – gradijent od donje ka gornjoj boji
uniform int  uMode;

uniform vec3 uSkyTop;
uniform vec3 uSkyBottom;

void main()
{
    if (uMode == 2) {
        // NEBO: vWorldPos.y je u [-1, 1] -> preslikamo u [0,1]
        float t = (vWorldPos.y + 1.0) * 0.5;
        vec3 col = mix(uSkyBottom, uSkyTop, t);
        outColor = vec4(col, 1.0);
    }
    else if (uMode == 1) {
        // BLAGO OSVETLJENJE: svetlije gore, tamnije dole
        float shade = 0.7 + 0.3 * (vWorldPos.y + 1.0) * 0.5; // u [0.7, 1.0]
        vec3 col = vColor * shade;
        outColor = vec4(col, 1.0);
    }
    else {
        // RAVNO
        outColor = vec4(vColor, 1.0);
    }
}
