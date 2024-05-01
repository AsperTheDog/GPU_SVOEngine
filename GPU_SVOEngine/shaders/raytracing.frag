#version 450

#extension GL_KHR_vulkan_glsl : enable

#define PI 3.14159265359

layout ( push_constant ) uniform PushConstants {
	vec4 camPos;
    vec3 camDir;
    mat4 invPVMatrix;

    vec3 sunDirection;

    vec3 skyColor;
    vec3 sunColor;

    float octreeScale;

    float brightness;
    float saturation;
    float contrast;
    float gamma;
};

struct Material {
   vec3 color;
   float roughness;
   float metallic;
   float ior;
   float transmission;
   float emission;
   uint albedoMap;
   uint normalMap;
};

layout(set = 0, binding = 0) buffer OctreeData {
  uint octree[];
};

layout(set = 0, binding = 1) buffer MaterialData {
  Material materials[];
};

layout(set = 0, binding = 2) uniform sampler2D tex[SAMPLER_ARRAY_SIZE];

layout(location = 0) in vec2 fragScreenCoord;

layout(location = 0) out vec4 outColor;

//*********************
//  OCTREE TRAVERSAL
//*********************

struct BranchNode 
{
    uint address;
    uint farFlag;
    uint childMask;
    uint leafMask;
};

struct LeafNode 
{
    uint material;
    vec3 normal;
    vec2 uv;
};

struct Node
{
    uint data;
    bool isLeaf;
};

struct Collision 
{
    bool hit;
    LeafNode voxel;
    vec3 voxelPos;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
    vec3 invDirection;
    float t;
    Collision coll;
#ifdef INTERSECTION_TEST
    float testTint;
#endif
};

struct StackElem
{
    uint index;
    vec3 pos;
    uint childCount;
};

vec3 childPositions[] = {
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(0, 1, 0),
    vec3(0, 1, 1),
    vec3(1, 0, 0),
    vec3(1, 0, 1),
    vec3(1, 1, 0),
    vec3(1, 1, 1)
};

uint orders[8][8] = {
    {7, 6, 5, 3, 4, 2, 1, 0}, // 0, 0, 0
	{3, 2, 7, 1, 0, 6, 5, 4}, // 0, 0, 1
    {5, 1, 7, 4, 3, 6, 0, 2}, // 0, 1, 0
	{1, 0, 3, 5, 4, 2, 7, 6}, // 0, 1, 1
    {6, 7, 2, 4, 5, 3, 0, 1}, // 1, 0, 0
	{2, 6, 3, 0, 4, 7, 1, 5}, // 1, 0, 1
    {4, 5, 6, 0, 7, 2, 1, 3}, // 1, 1, 0
	{0, 4, 2, 1, 6, 3, 5, 7}  // 1, 1, 1
};

vec3 homogenize(vec4 p)
{
    return p.xyz / p.w;
}


BranchNode parseBranch(uint node)
{
	BranchNode n;
    n.farFlag =   (node & 0x80000000) >> 31;
    n.address =   (node & 0x7FFF0000) >> 16;
    n.childMask = (node & 0x0000FF00) >> 8;
    n.leafMask =  (node & 0x000000FF);
	return n;
}

LeafNode parseLeaf(uint node1, uint node2)
{
	LeafNode n;
	n.uv.x = float((node1 & 0xFFF00000) >> 20) / 0x0FFF;
    n.uv.y = float((node1 & 0x000FFF00) >> 8) / 0x0FFF;
    n.material = ((node1 & 0x000000FF) << 2) | ((node2 & 0xC0000000) >> 30);
    n.normal.x = float((node2 & 0x3FF00000) >> 20);
    n.normal.y = float((node2 & 0x000FFC00) >> 10);
    n.normal.z = float(node2 & 0x000003FF);
    n.normal = n.normal / 0x1FF - 1.0;
    if (n.normal.x != 0.0 || n.normal.y != 0.0 || n.normal.z != 0.0)
        n.normal = normalize(n.normal);
	return n;
}

bool intersect(Ray ray, vec3 boxMin, vec3 boxMax)
{
    float t1 = (boxMin.x - ray.origin.x) * ray.invDirection.x;
    float t2 = (boxMax.x - ray.origin.x) * ray.invDirection.x;
    float t3 = (boxMin.y - ray.origin.y) * ray.invDirection.y;
    float t4 = (boxMax.y - ray.origin.y) * ray.invDirection.y;
    float t5 = (boxMin.z - ray.origin.z) * ray.invDirection.z;
    float t6 = (boxMax.z - ray.origin.z) * ray.invDirection.z;
	
    float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
    float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

    float finalT = tmin <= 0 ? tmax : tmin;
    bool res = tmax >= tmin && finalT > 0;

    return res;
}

uint getOctact(Ray ray)
{
    uint octant = 0;
    if (ray.direction.x > 0) octant |= 1;
    if (ray.direction.y > 0) octant |= 2;
    if (ray.direction.z > 0) octant |= 4;

    return octant;
}

uint getNextChild(inout StackElem stackElem, uint octant)
{
    BranchNode node = parseBranch(octree[stackElem.index]);
    if (node.childMask == 0) return 8;
    while (stackElem.childCount < 8)
    {
        uint next = orders[octant][stackElem.childCount];
        if ((node.childMask & (1 << next)) != 0) return next;
        stackElem.childCount++;
    }
    return 8;
}

uint getMemoryPosOfChild(BranchNode node, uint child)
{
    uint count = 0;
    for (uint i = 0; i < child; i++)
    {
        if ((node.childMask & (1 << i)) != 0) 
        {
            if ((node.leafMask & (1 << i)) != 0) count += 2;
            else count++;
        }
    }
    return count;
}

void traceRay(inout Ray ray)
{
    float halfScale = octreeScale / 2.0;
    StackElem[20] stack;
    stack[0] = StackElem(0, vec3(-halfScale), 0);
    int stackPtr = 0;

    uint octant = getOctact(ray);
    if (!intersect(ray, vec3(-halfScale), vec3(halfScale))) return;

    while (true)
    {
        uint current = getNextChild(stack[stackPtr], octant);

        if (current > 7)
        {
            stackPtr--;
            if (stackPtr < 0) break;
            stack[stackPtr].childCount++;
            continue;
        }
        float size = pow(2.0, -(stackPtr + 1)) * octreeScale;
        vec3 pos = stack[stackPtr].pos + size * childPositions[current];
#ifdef INTERSECTION_TEST
        ray.testTint += 0.0025;
#endif
        if (intersect(ray, pos, pos + vec3(size)))
        {
            BranchNode parent = parseBranch(octree[stack[stackPtr].index]);
            uint absAddress = stack[stackPtr].index + parent.address;
            uint trueAddress = parent.farFlag == 0 ? absAddress : absAddress + octree[absAddress];
            uint nextAddress = trueAddress + getMemoryPosOfChild(parent, current);
            if ((parent.leafMask & (1 << current)) != 0)
            {
                ray.coll = Collision(true, parseLeaf(octree[nextAddress], octree[nextAddress + 1]), pos + vec3(size) / 2.0);
                break;
            }
            else
            {
                stackPtr++;
                stack[stackPtr] = StackElem(nextAddress, pos, 0);
                continue;
            }
        }
        else
        {
            stack[stackPtr].childCount++;
        }
    }
}

//*********************
//     LIGHTING
//*********************

vec3 colorCorrection(vec3 color)
{
    color = contrast * (color - 0.5f) + 0.5f + brightness;
	color = clamp(color, vec3(0.0), vec3(1.0));

	vec3 desat = vec3(dot(color, vec3(0.299, 0.587, 0.114))); // Luminance
	color = mix(desat, color, saturation);

	color = clamp(color, vec3(0.0), vec3(1.0));

	color = pow(color.rgb, vec3(gamma));
	color = clamp(color, vec3(0.0), vec3(1.0));

	return color;
}

vec3 calculateLighting(Material mat, vec2 uv, vec3 normal, vec3 position)
{
    vec3 norm_sunDirection = normalize(sunDirection);
    vec3 norm_normal = normalize(normal);
    vec3 norm_camDir = normalize(camDir);
	vec3 reflectDir = reflect(-norm_sunDirection, norm_normal);

    vec3 color = vec3(1.0, 1.0, 1.0);
    if (mat.albedoMap < SAMPLER_ARRAY_SIZE) 
        color *= texture(tex[mat.albedoMap], uv).rgb;

    vec3 amb = color * 0.05;
    float diff = clamp(dot(norm_normal, norm_sunDirection), 0.0, 1.0);
    vec3 halfV = normalize(norm_sunDirection + norm_camDir);
    float shininess = clamp(sqrt(2.0 / (mat.roughness + 2)), 0.0, 1.0);
    float spec = pow(max(dot(norm_normal, halfV), 0.0), shininess * 4);

    vec3 ambient = sunColor * amb * color;
    vec3 diffuse = sunColor * diff * color;
    vec3 specular = sunColor * spec * color;

    return (amb + diffuse + specular);
}

//*********************
//        MAIN
//*********************

void main() {
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - ray.origin);
    ray.invDirection = 1.0 / ray.direction;
    ray.t = 0.0;
    ray.coll = Collision(false, parseLeaf(0, 0), vec3(0, 0, 0));
#ifdef INTERSECTION_TEST
    ray.testTint = 0.0;
#endif

    traceRay(ray);

    if (ray.coll.hit)
    {
#ifndef INTERSECTION_TEST
        vec3 shaded = calculateLighting(materials[ray.coll.voxel.material], ray.coll.voxel.uv, ray.coll.voxel.normal, ray.coll.voxelPos);
        outColor = vec4(colorCorrection(shaded), 1.0);
#else
    #ifdef INTERSECTION_COLOR
        outColor = vec4(ray.testTint, 1 - ray.testTint, 0.0, 1.0);
    #else
        outColor = vec4(vec3(ray.testTint), 1.0);
    #endif
#endif
    }
    else
    {
#ifdef INTERSECTION_TEST
    #ifdef INTERSECTION_COLOR
        outColor = vec4(ray.testTint, 0.0, 0.0, 1.0);
    #else
        outColor = vec4(vec3(ray.testTint), 1.0);
    #endif
#else
        outColor = vec4(skyColor, 1.0);
#endif
    }
}