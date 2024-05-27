#version 450

#extension GL_KHR_vulkan_glsl : enable

layout ( push_constant ) uniform PushConstants {
	vec3 camPos;
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
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float specularComp;
    uint diffuseMap;
    uint normalMap;
    uint specularMap;
};

layout(set = 0, binding = 0) buffer OctreeData {
  uint octree[];
};

layout(set = 0, binding = 1) buffer MaterialData {
  Material materials[];
};

layout(set = 0, binding = 2) uniform sampler2D tex[SAMPLER_ARRAY_SIZE]; // SAMPLER_ARRAY_SIZE defined in the C++ code at runtime

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

struct Collision 
{
    bool hit;
    uint voxelIndex;
    vec3 voxelPos;

    #define NULL_COLLISION Collision(false, 0, vec3(0.0))
};

struct Ray
{
    vec3 origin;
    vec3 direction;
    vec3 invDirection;
#ifdef INTERSECTION_TEST
    float testTint;
#endif
};

struct StackElem
{
    uint index;
    vec3 pos;
    uint childCount;
    uint intersectionMask;
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
	n.uv.x =     float((node1 & 0xFFF00000) >> 20);
    n.uv.y =     float((node1 & 0x000FFF00) >> 8);
    n.material =      ((node1 & 0x000000FF) << 2);
    n.material |=     ((node2 & 0xC0000000) >> 30);
    n.normal.x = float((node2 & 0x3FF00000) >> 20);
    n.normal.y = float((node2 & 0x000FFC00) >> 10);
    n.normal.z =  float(node2 & 0x000003FF);

    n.uv = n.uv / 0xFFF;
    n.normal = n.normal / 0x1FF - 1.0;

    if (n.normal.x != 0.0 || n.normal.y != 0.0 || n.normal.z != 0.0)
        n.normal = normalize(n.normal);

	return n;
}

uint getNextChild(inout StackElem stackElem, uint octant)
{
    BranchNode node = parseBranch(octree[stackElem.index]);
    if (node.childMask == 0) return 8;
    while (stackElem.childCount < 8)
    {
        uint next = stackElem.childCount ^ octant;
        if ((node.childMask & (1 << next)) != 0 && (stackElem.intersectionMask & (1 << next)) != 0) return next;
        stackElem.childCount++;
    }
    return 8;
}

uint createIntersectionMask(Ray ray, vec3 boxMin, vec3 boxMax)
{
    float nodeRadius = (boxMax.x - boxMin.x) / 2.0;
    vec3 nodeCenter = boxMin + nodeRadius;

    vec3 tMid = (nodeCenter - ray.origin) * ray.invDirection;

    vec3 slabRadius = nodeRadius * abs(ray.invDirection);
    vec3 tMin = tMid - slabRadius;
    float rayTMin = max(max(max(tMin.x, tMin.y), tMin.z), 0.0);
    vec3 tMax = tMid + slabRadius;
    float rayTMax = min(min(tMax.x, tMax.y), tMax.z);

    uint intersectionMask = 0;
    uint firstChildHit;
    vec3 pointOnRay = ray.origin + 0.5 * (rayTMin + rayTMax) * ray.direction;
	firstChildHit = pointOnRay.x >= nodeCenter.x ? 4 : 0;
	firstChildHit += pointOnRay.y >= nodeCenter.y ? 2 : 0;
	firstChildHit += pointOnRay.z >= nodeCenter.z ? 1 : 0;
    intersectionMask |= (1 << firstChildHit);

    const float epsilon = 0.0001f;
	vec3 pointOnRaySegment0 = ray.origin + tMid.x * ray.direction;
	uint A = (abs(nodeCenter.y - pointOnRaySegment0.y) < epsilon) ? (0xCC | 0x33) : ((pointOnRaySegment0.y >= nodeCenter.y ? 0xCC : 0x33));
	uint B = (abs(nodeCenter.z - pointOnRaySegment0.z) < epsilon) ? (0xAA | 0x55) : ((pointOnRaySegment0.z >= nodeCenter.z ? 0xAA : 0x55));
	intersectionMask |= (tMid.x < rayTMin || tMid.x > rayTMax) ? 0 : (A & B);
	vec3 pointOnRaySegment1 = ray.origin + tMid.y * ray.direction;
	uint C = (abs(nodeCenter.x - pointOnRaySegment1.x) < epsilon) ? (0xF0 | 0x0F) : ((pointOnRaySegment1.x >= nodeCenter.x ? 0xF0 : 0x0F));
	uint D = (abs(nodeCenter.z - pointOnRaySegment1.z) < epsilon) ? (0xAA | 0x55) : ((pointOnRaySegment1.z >= nodeCenter.z ? 0xAA : 0x55));
	intersectionMask |= (tMid.y < rayTMin || tMid.y > rayTMax) ? 0 : (C & D);
	vec3 pointOnRaySegment2 = ray.origin + tMid.z * ray.direction;
	uint E = (abs(nodeCenter.x - pointOnRaySegment2.x) < epsilon) ? (0xF0 | 0x0F) : ((pointOnRaySegment2.x >= nodeCenter.x ? 0xF0 : 0x0F));
	uint F = (abs(nodeCenter.y - pointOnRaySegment2.y) < epsilon) ? (0xCC | 0x33) : ((pointOnRaySegment2.y >= nodeCenter.y ? 0xCC : 0x33));
	intersectionMask |= (tMid.z < rayTMin || tMid.z > rayTMax) ? 0 : (E & F);

    return intersectionMask;
}

Collision traceRay(inout Ray ray, uint octant)
{
    float halfScale = octreeScale / 2.0;
    StackElem[OCTREE_DEPTH] stack;
    stack[0] = StackElem(0, vec3(-halfScale), 0, createIntersectionMask(ray, vec3(-halfScale), vec3(halfScale)));
    int stackPtr = 0;

    while (true)
    {
        uint current = getNextChild(stack[stackPtr], octant);
        if (current > 7)
        {
            // POP
            stackPtr--;
            if (stackPtr < 0) break;
            stack[stackPtr].childCount++;
            continue;
        }
#ifdef INTERSECTION_TEST
        ray.testTint += 0.0025;
#endif
        BranchNode parent = parseBranch(octree[stack[stackPtr].index]);
        uint nextChild;
        {
            uint bitMask = (1 << current) - 1;
            uint childOffset = bitCount(parent.childMask & bitMask) + bitCount(parent.leafMask & bitMask & parent.childMask);
            uint nextAbsAddress = stack[stackPtr].index + parent.address;
            uint resolvedAddress = parent.farFlag == 0 ? nextAbsAddress : nextAbsAddress + octree[nextAbsAddress];
            nextChild = resolvedAddress + childOffset;
        }
        float size = pow(2.0, -(stackPtr + 1)) * octreeScale;
        vec3 pos = stack[stackPtr].pos + size * vec3((current & 4) >> 2, (current & 2) >> 1, current & 1);
        if ((parent.leafMask & (1 << current)) != 0)
        {
            LeafNode voxel = parseLeaf(octree[nextChild], octree[nextChild + 1]);
            if (materials[voxel.material].diffuseMap < SAMPLER_ARRAY_SIZE && texture(tex[materials[voxel.material].diffuseMap], voxel.uv).a >= 0.1)
                return Collision(true, nextChild, pos + vec3(size) / 2.0);
            stack[stackPtr].childCount++;
            continue;
        }
        else
        {
            // PUSH
            stackPtr++;
            stack[stackPtr] = StackElem(nextChild, pos, 0, createIntersectionMask(ray, pos, pos + vec3(size)));
            continue;
        }
    }
    return NULL_COLLISION;
}

uint getOctant(vec3 direction)
{
    uint octant = 0;
    if (direction.x < 0) octant |= 4;
    if (direction.y < 0) octant |= 2;
    if (direction.z < 0) octant |= 1;
    return octant;
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

vec3 calculateLighting(Collision coll)
{
    LeafNode voxel = parseLeaf(octree[coll.voxelIndex], octree[coll.voxelIndex + 1]);
    Material mat = materials[voxel.material];

    vec3 diffAmbTexel = vec3(1.0);
    if (mat.diffuseMap < SAMPLER_ARRAY_SIZE) 
        diffAmbTexel = texture(tex[mat.diffuseMap], voxel.uv).rgb;
    vec3 ambientColor = mat.ambient * diffAmbTexel;
    
    float amb = 0.1;
    vec3 ambient = sunColor * amb * ambientColor;

#ifndef NO_SHADOW
    Ray shadowRay;
    shadowRay.direction = normalize(sunDirection);
    shadowRay.origin = coll.voxelPos + VOXEL_SIZE * octreeScale * voxel.normal;
    shadowRay.invDirection = 1.0 / shadowRay.direction;
    if (traceRay(shadowRay, getOctant(shadowRay.direction)).hit)
    {
        return ambient;
    }
#endif

    vec3 norm_sunDirection = normalize(sunDirection);
    vec3 norm_camDir = normalize(camPos - coll.voxelPos);
    vec3 halfV = normalize(norm_sunDirection + norm_camDir);
    float specularTexel = 1.0;
    if (mat.specularMap < SAMPLER_ARRAY_SIZE) 
        specularTexel = texture(tex[mat.specularMap], voxel.uv).r;

    vec3 diffuseColor = mat.diffuse * diffAmbTexel;
    vec3 specularColor = mat.specular * specularTexel;

    float diff = clamp(dot(voxel.normal, norm_sunDirection), 0.0, 1.0);
    float tmp = max(dot(voxel.normal, halfV), 0.0);
    float spec = tmp == 0 || mat.specularComp == 0 ? 0 : pow(tmp, mat.specularComp * 3);

    vec3 diffuse = sunColor * diff * diffuseColor;
    vec3 specular = sunColor * spec * specularColor;

    return (ambient + diffuse + specular);
}

//*********************
//        MAIN
//*********************

void main() {
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - ray.origin);
    ray.invDirection = 1.0 / ray.direction;
#ifdef INTERSECTION_TEST
    ray.testTint = 0.0;
#endif

    Collision coll = traceRay(ray, getOctant(ray.direction));

    if (coll.hit)
    {
#ifndef INTERSECTION_TEST
        vec3 shaded = calculateLighting(coll);
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