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

    float fadeX =
        smoothstep(0.0, s, local.x) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeY =
        smoothstep(0.0, s, local.y) *
        (1.0 - smoothstep(1.0 - s, 1.0, local.y));

    float mask = fadeX * fadeY;

    vec2 safeScale = max(uUvScale, vec2(0.0001));
    vec2 rippleUv = local * safeScale;
    rippleUv += uNoiseScrollSpeed * uTime;
    rippleUv += vec2(uPhaseOffset, uPhaseOffset);

    float n1 = noise(rippleUv + vec2(0.0, 0.0));
    float n2 = noise(rippleUv + vec2(11.7, 5.3));

    float wave1 = sin((rippleUv.x + rippleUv.y * 0.35 + uTime * 0.9 + uPhaseOffset) * 6.28318);
    float wave2 = sin((rippleUv.x * 0.6 - rippleUv.y + uTime * 0.7 + uPhaseOffset * 1.7) * 6.28318);

    vec2 offsetPixels;
    offsetPixels.x =
        ((n1 * 2.0 - 1.0) * 0.55 + wave1 * 0.45) * uDistortionAmount.x;
    offsetPixels.y =
        ((n2 * 2.0 - 1.0) * 0.55 + wave2 * 0.45) * uDistortionAmount.y;

    float centerBoost =
        smoothstep(0.0, 0.25, local.y) *
        (1.0 - smoothstep(0.85, 1.0, local.y));

    float rippleMask = mask * centerBoost * uIntensity;

    vec2 offsetUv = (offsetPixels / uSceneSize) * rippleMask;
    vec2 sampleUv = fragTexCoord + offsetUv;

    finalColor = texture(texture0, sampleUv);
}

