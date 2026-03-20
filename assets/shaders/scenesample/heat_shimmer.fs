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

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x)
         + (c - a) * u.y * (1.0 - u.x)
         + (d - b) * u.x * u.y;
}

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    bool insideRect =
        local.x >= 0.0 && local.x <= 1.0 &&
        local.y >= 0.0 && local.y <= 1.0;

    bool insideShape = insideRect && pointInPolygon(pixelPos);

    if (!insideShape) {
        finalColor = texture(texture0, fragTexCoord);
        return;
    }

    vec2 safeScale = max(uUvScale, vec2(0.0001));
    vec2 noiseUv = local * safeScale;
    noiseUv += uNoiseScrollSpeed * uTime;
    noiseUv += vec2(uPhaseOffset, uPhaseOffset);

    float n1 = noise(noiseUv + vec2(0.0, 0.0));
    float n2 = noise(noiseUv + vec2(17.3, 9.1));

    vec2 offsetPixels = vec2(
        (n1 * 2.0 - 1.0) * uDistortionAmount.x,
        (n2 * 2.0 - 1.0) * uDistortionAmount.y
    );

    float fadeLeftRight =
        smoothstep(0.0, 0.22, local.x) *
        (1.0 - smoothstep(0.78, 1.0, local.x));

    float fadeTopBottom =
        smoothstep(0.0, 0.10, local.y) *
        (1.0 - smoothstep(0.70, 1.0, local.y));

    float centerBias = smoothstep(0.1, 0.6, local.y);

    float shimmerMask = fadeLeftRight * fadeTopBottom * centerBias;

    vec2 offsetUv = (offsetPixels / uSceneSize) * shimmerMask * uIntensity;
    vec2 sampleUv = fragTexCoord + offsetUv;

    finalColor = texture(texture0, sampleUv);
}
