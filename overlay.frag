#version 330 core

in vec2 vUV;
out vec4 outColor;

uniform sampler2D uTex;

void main()
{
    outColor = texture(uTex, vUV);
}
