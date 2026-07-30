#version 330

// Veil forward lighting — fragment stage
//
// One pass, up to MAX_LIGHTS lights, plus a single directional shadow map,
// distance fog and an exposure control.
//
// MAX_LIGHTS must stay equal to LIGHT_LIMIT in LightingSystem.h. The C++ side
// always uploads the full array (zero-filled tail) and a separate lightCount,
// so a shrinking light list can never leave stale data in the unused slots.

#define MAX_LIGHTS 8

// Must match enum LightType in LightingSystem.h
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Filled in by raylib's DrawMesh() from the object's Material
uniform sampler2D texture0; // material.maps[MATERIAL_MAP_DIFFUSE].texture
uniform vec4 colDiffuse;    // the `tint` argument of DrawModel() = GameObject::defaultColor

// Camera world position. NOTE: raylib does *not* auto-resolve this name, so
// LightingSystem::Init() assigns shader.locs[SHADER_LOC_VECTOR_VIEW] by hand.
uniform vec3 viewPos;

// Light block — parallel arrays rather than an array of structs, because
// SetShaderValueV() can upload each one in a single glUniform*v call.
uniform int   lightCount;
uniform int   lightType[MAX_LIGHTS];
uniform vec3  lightPosition[MAX_LIGHTS];
uniform vec3  lightDirection[MAX_LIGHTS];  // pre-normalized on the CPU
uniform vec4  lightColor[MAX_LIGHTS];      // rgb = colour, a = intensity
uniform float lightRange[MAX_LIGHTS];      // distance at which the light reaches exactly 0
uniform vec2  lightCone[MAX_LIGHTS];       // x = cos(innerAngle), y = cos(outerAngle)

// Atmosphere
uniform vec4  ambient;          // rgb = colour, a = strength
uniform vec4  fogColor;
uniform float fogDensity;
uniform float exposure;
uniform float specularStrength;
uniform float shininess;

// Shadow block
uniform sampler2D shadowMap;      // depth texture, bound to a high texture unit by BindShadowMap()
uniform mat4  lightVP;            // view*projection of the shadow light's camera
uniform float shadowTexelSize;    // 1.0 / shadowMapResolution
uniform float shadowBias;
uniform float shadowSoftness;     // PCF tap spacing multiplier
uniform int   shadowsEnabled;
uniform int   shadowLightIndex;   // which lights[] slot owns the shadow map, -1 for none

// 1 while LightingSystem::RenderShadowPass() is filling the depth texture.
// See the early-out in main() — this is a correctness flag, not an optimisation.
uniform int   depthPass;

out vec4 finalColor;

// Reproject the fragment into the shadow light's clip space and compare depths.
// Returns 1.0 for fully lit, 0.0 for fully shadowed.
float ComputeShadow(float NdotL)
{
    vec4 lightClip = lightVP*vec4(fragPosition, 1.0);

    // Perspective divide then NDC [-1,1] -> texture space [0,1]
    vec3 proj = lightClip.xyz/lightClip.w;
    proj = proj*0.5 + 0.5;

    // Outside the light camera's frustum there is simply no depth information.
    // Reporting "shadowed" here would paint everything past shadowDistance
    // solid black, so unknown fragments are treated as lit.
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) { return 1.0; }

    // Slope-scaled bias: a surface seen edge-on by the light spans many depth
    // values inside one shadow-map texel and self-shadows ("acne") without a
    // larger offset. The floor keeps face-on surfaces from peter-panning.
    float bias = max(shadowBias*(1.0 - NdotL), shadowBias*0.1);

    // 3x3 PCF. Averaging 9 binary comparisons turns the shadow map's hard
    // texel staircase into a soft edge for the cost of 9 texture fetches.
    float lit = 0.0;
    float tapStep = shadowTexelSize*max(shadowSoftness, 0.0);

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float mapDepth = texture(shadowMap, proj.xy + vec2(float(x), float(y))*tapStep).r;
            lit += (proj.z - bias > mapDepth) ? 0.0 : 1.0;
        }
    }

    return lit/9.0;
}

void main()
{
    // --- Depth pass early-out -------------------------------------------------
    // RenderShadowPass() reuses this very shader to fill the depth texture, so
    // for those draws shadowMap is bound as the render target *and* readable as
    // a sampler. Sampling it then is undefined in OpenGL, so bail out before
    // any lighting or shadow work happens. Only gl_FragDepth matters here.
    if (depthPass == 1)
    {
        finalColor = vec4(1.0);
        return;
    }

    // Base surface colour. colDiffuse carries GameObject::defaultColor, so the
    // existing per-object tinting keeps working unchanged.
    vec4 albedo = texture(texture0, fragTexCoord)*colDiffuse*fragColor;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);

    vec3 lightAccum = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        // Unused slots cost nothing; the tail of the array is zero-filled but
        // never evaluated.
        if (i >= lightCount) { break; }

        vec3 lightDir;
        float attenuation = 1.0;

        if (lightType[i] == LIGHT_DIRECTIONAL)
        {
            // Direction points the way the light travels, so the vector *to*
            // the light is its negation. No falloff — the source is at infinity.
            lightDir = -normalize(lightDirection[i]);
        }
        else
        {
            vec3 toLight = lightPosition[i] - fragPosition;
            float lightDist = length(toLight);
            lightDir = (lightDist > 0.0001) ? toLight/lightDist : vec3(0.0, 1.0, 0.0);

            // Art-directed falloff, not physical inverse-square: exactly 1.0 at
            // the source and exactly 0.0 at `range`. A 1/d^2 term would need
            // intensity values in the hundreds and would make `range` — the
            // number a level designer actually authors — meaningless.
            float ratio = clamp(lightDist/max(lightRange[i], 0.0001), 0.0, 1.0);
            float falloff = 1.0 - ratio*ratio;
            attenuation = falloff*falloff;

            if (lightType[i] == LIGHT_SPOT)
            {
                // theta is 1.0 dead centre in the cone and shrinks outward.
                // cos() decreases as the angle grows, so cos(outer) is the LOW
                // edge of the smoothstep and cos(inner) the high edge.
                float theta = dot(lightDir, -normalize(lightDirection[i]));
                attenuation *= smoothstep(lightCone[i].y, lightCone[i].x, theta);
            }
        }

        float NdotL = max(dot(normal, lightDir), 0.0);
        if (NdotL <= 0.0 || attenuation <= 0.0) { continue; }

        // Blinn-Phong specular: the half vector is cheaper than a reflect() and
        // stays stable at grazing angles.
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), max(shininess, 1.0))*specularStrength;

        // Only one light owns the shadow map this frame (see RenderShadowPass).
        float occlusion = 1.0;
        if (shadowsEnabled == 1 && i == shadowLightIndex)
        {
            occlusion = ComputeShadow(NdotL);
        }

        lightAccum += lightColor[i].rgb*lightColor[i].a*attenuation*occlusion*(NdotL + spec);
    }

    // Ambient keeps unlit geometry readable instead of pure black.
    vec3 lit = albedo.rgb*(ambient.rgb*ambient.a + lightAccum);

    // Exponential-squared distance fog: stays clear up close, then closes off
    // hard — the sightline control a horror scene wants. update_game() clears
    // the backbuffer to fogColor so the falloff blends into the background
    // instead of hitting a bright wall.
    float viewDistance = length(viewPos - fragPosition);
    float fogAmount = 1.0 - exp(-pow(viewDistance*fogDensity, 2.0));
    lit = mix(lit, fogColor.rgb, clamp(fogAmount, 0.0, 1.0));

    lit *= exposure;

    // No gamma encode on purpose: textures are loaded as plain
    // UNCOMPRESSED_R8G8B8A8 with no sRGB decode, so encoding on output without
    // decoding on input would wash every surface out. Going linear is an
    // engine-wide change (texture loading, the 2D/UI pass, every authored colour).
    finalColor = vec4(clamp(lit, 0.0, 1.0), albedo.a);
}
