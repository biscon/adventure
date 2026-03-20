#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float uTime;
uniform vec2 uScrollSpeed;
uniform vec2 uUvScale;
uniform vec2 uDistortionAmount;
uniform vec2 uNoiseScrollSpeed;
uniform float uIntensity;
uniform float uPhaseOffset;

uniform vec2 uSceneSize;
uniform vec2 uRegionPos;
uniform vec2 uRegionSize;
uniform float uSoftness;

out vec4 finalColor;

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    if (local.x < 0.0 || local.x > 1.0 || local.y < 0.0 || local.y > 1.0) {
        discard;
    }

    vec2 safeUvScale = max(uUvScale, vec2(0.0001));
    vec2 uv = fragTexCoord;
    uv *= safeUvScale;
    uv += uScrollSpeed * uTime;
    uv += vec2(uPhaseOffset, uPhaseOffset);

    vec4 texel = texture(texture0, uv);

    float s = clamp(uSoftness, 0.0001, 0.49);

    float fadeX =
        smoothstep(0.0, s, local.x) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeY =
        smoothstep(0.0, s, local.y) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.y));

    float mask = fadeX * fadeY;

    texel.a *= mask;

    finalColor = texel * colDiffuse * fragColor;
    finalColor.rgb *= finalColor.a;

    // alpha height fade - optional fades stronger toward transparent at the top
    float heightFade = smoothstep(0.0, 0.4, local.y);
    finalColor.a *= heightFade;
}