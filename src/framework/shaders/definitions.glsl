struct RayPayload {
    vec3 surfaceEmissive;
    vec3 surfaceRadiance;
    vec3 surfaceAttenuation;
    vec3 nextRayOrigin;
    vec3 nextRayDirection;
    uint seed;
    int done;
    int rayType; // RAY_TYPE_DIFFUSE 1, RAY_TYPE_REFRACTION 1, RAY_TYPE_REFLECTION 2, RAY_TYPE_MISS
                 // 3
    float hitDistance;
};

struct RayPayloadShadow {
    float shadowAmount;
};

struct ShaderMeshInstance {
    uint indexBase;
    uint vertexBase;
    uint materialIndex;
};

struct MaterialProperties {
    vec3 ambient; // 1 2 3
    float pad0; // 4
    vec3 diffuse; // 1 2 3
    float pad1; // 4
    vec3 specular; // 1 2 3
    float pad2; // 4
    vec3 emissive; // 1 2 3
    float pad3; // 4
    int diffuseMapIndex; // 1
    int normalMapIndex; // 2
    int specularMapIndex; // 3
    int emissiveMapIndex; // 4
    float opacity; // 1
    float reflectivity; // 2
    float refractIdx; // 3
    float shininessStrength; // 4
};

struct LightProperties {
    vec3 position; // 1 2 3
    float pad0; // 4
    vec3 direction; // 1 2 3
    float pad1; // 4
    vec3 diffuse; // 1 2 3
    float pad2; // 4
    vec3 specular; // 1 2 3
    float pad3; // 4
    int lightType; // 1
    uint areaInstanceId; // 2
    uint areaPrimitiveCount; // 3
    uint areaMaterialIdx; // 4
};

const vec3 SUN_POWER = vec3(1.0, 0.9, 0.7);
