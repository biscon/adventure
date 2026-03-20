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

uniform int uUsePolygon;
uniform int uPolygonVertexCount;
uniform vec2 uPolygonPoints[32];

out vec4 finalColor;

bool pointInPolygon(vec2 p)
{
    if (uUsePolygon == 0 || uPolygonVertexCount < 3) {
        return true;
    }

    bool inside = false;

    for (int i = 0, j = uPolygonVertexCount - 1; i < uPolygonVertexCount; j = i, ++i) {
        vec2 a = uPolygonPoints[i];
        vec2 b = uPolygonPoints[j];

        float denom = b.y - a.y;
        if (abs(denom) < 0.00001) {
            denom = (denom < 0.0) ? -0.00001 : 0.00001;
        }

        bool intersect =
            ((a.y > p.y) != (b.y > p.y)) &&
            (p.x < ((b.x - a.x) * (p.y - a.y) / denom) + a.x);

        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

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

    bool insideRect =
        local.x >= 0.0 && local.x <= 1.0 &&
        local.y >= 0.0 && local.y <= 1.0;

    bool insidePoly = pointInPolygon(pixelPos);

    if (!insideRect || !insidePoly) {
        finalColor = original;
        return;
    }

    float s = clamp(uSoftness, 0.0001, 0.49);

    float fadeLeftRight =
        smoothstep(0.0, s, local.x) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeTopBottom =
        smoothstep(0.0, s, local.y) *
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
