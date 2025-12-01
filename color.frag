#version 330 core

in vec4 chCol;
out vec4 outCol;

uniform float uB;              // dodatak na plavu komponentu
uniform float uAlpha;          // providnost kad je ukljucimo
uniform bool  useAlphaUniform; // da li da koristimo uAlpha ili chCol.a

void main()
{
    float alpha = useAlphaUniform ? uAlpha : chCol.a;
    outCol = vec4(chCol.r, chCol.g, chCol.b + uB, alpha);
}
