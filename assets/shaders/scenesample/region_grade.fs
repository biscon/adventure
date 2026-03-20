#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 uSceneSize;
uniform vec2 uRegionPos;
uniform vec2 uRegionSize;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform vec3 uTint;
uniform float uSoftness;

out vec4 finalColor;

vec3 applySaturation(vec3 color, float saturation)
{
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luma), color, saturation);
}

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    vec4 original = texture(texture0, fragTexCoord);

    if (local.x < 0.0 || local.x > 1.0 || local.y < 0.0 || local.y > 1.0) {
        finalColor = original;
        return;
    }

    float s = clamp(uSoftness, 0.0001, 0.49);

    float fadeLeftRight = smoothstep(0.0, s, local.x) *
                          (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeTopBottom = smoothstep(0.0, s, local.y) *
                          (1.0 - smoothstep(1.0 - s, 1.0, local.y));

    float mask = fadeLeftRight * fadeTopBottom;

    vec3 graded = original.rgb;

    graded += vec3(uBrightness);
    graded = ((graded - 0.5) * uContrast) + 0.5;
    graded = applySaturation(graded, uSaturation);
    graded *= uTint;

    graded = clamp(graded, 0.0, 1.0);

    finalColor = vec4(mix(original.rgb, graded, mask), original.a);
}
