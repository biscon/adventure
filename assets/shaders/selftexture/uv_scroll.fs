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

float distanceToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 ab = b - a;
    float abLenSq = dot(ab, ab);
    if (abLenSq <= 0.00001) {
        return length(p - a);
    }

    float t = clamp(dot(p - a, ab) / abLenSq, 0.0, 1.0);
    vec2 closest = a + ab * t;
    return length(p - closest);
}

float polygonEdgeFade(vec2 pixelPos, float softnessPixels)
{
    if (uUsePolygon == 0 || uPolygonVertexCount < 3) {
        return 1.0;
    }

    float minDist = 1e20;

    for (int i = 0; i < uPolygonVertexCount; ++i) {
        int j = (i + 1) % uPolygonVertexCount;
        float d = distanceToSegment(pixelPos, uPolygonPoints[i], uPolygonPoints[j]);
        minDist = min(minDist, d);
    }

    return clamp(minDist / max(softnessPixels, 0.0001), 0.0, 1.0);
}

float rectEdgeFade(vec2 local, float softness)
{
    float s = clamp(softness, 0.0001, 0.49);

    float fadeX =
        smoothstep(0.0, s, local.x) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeY =
        smoothstep(0.0, s, local.y) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.y));

    return fadeX * fadeY;
}

float regionMask(vec2 pixelPos, vec2 local, float softness)
{
    if (uUsePolygon != 0 && uPolygonVertexCount >= 3) {
        if (!pointInPolygon(pixelPos)) {
            return 0.0;
        }

        float softnessPixels = max(uRegionSize.x, uRegionSize.y) * clamp(softness, 0.0001, 1.0);
        return polygonEdgeFade(pixelPos, softnessPixels);
    }

    if (local.x < 0.0 || local.x > 1.0 || local.y < 0.0 || local.y > 1.0) {
        return 0.0;
    }

    return rectEdgeFade(local, softness);
}

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    float mask = regionMask(pixelPos, local, uSoftness);
    if (mask <= 0.0001) {
        discard;
    }

    vec2 safeUvScale = max(uUvScale, vec2(0.0001));
    vec2 uv = fragTexCoord * safeUvScale;
    uv += uScrollSpeed * uTime;
    uv += vec2(uPhaseOffset, uPhaseOffset);

    vec4 texel = texture(texture0, uv);

    texel.a *= mask;

    finalColor = texel * colDiffuse * fragColor;
    finalColor.rgb *= finalColor.a;

    float heightFade = smoothstep(0.0, 0.4, local.y);
    finalColor.a *= heightFade;
    finalColor.rgb *= heightFade;
}
