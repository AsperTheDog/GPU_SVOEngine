#version 450

layout ( push_constant ) uniform PushConstants {
	vec4 camPos;
    mat4 invPVMatrix;
    float octreeScale;
};

layout(binding = 0) buffer MyBuffer {
  uint octree[];
};

layout(location = 0) in vec2 fragScreenCoord;

layout(location = 0) out vec4 outColor;

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
};

struct Ray
{
    vec3 origin;
    vec3 direction;
    vec3 invDirection;
    float t;
    Collision coll;
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
    n.material = (node1 & 0x000000FF) + ((node2 & 0xC0000000) >> 30);
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
    StackElem[20] stack;
    stack[0] = StackElem(0, vec3(0), 0);
    int stackPtr = 0;

    uint octant = getOctact(ray);
    if (!intersect(ray, vec3(0), vec3(octreeScale))) return;

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
        if (intersect(ray, pos, pos + vec3(size)))
        {
            BranchNode parent = parseBranch(octree[stack[stackPtr].index]);
            uint absAddress = stack[stackPtr].index + parent.address;
            uint trueAddress = parent.farFlag == 0 ? absAddress : absAddress + octree[absAddress];
            uint nextAddress = trueAddress + getMemoryPosOfChild(parent, current);
            if ((parent.leafMask & (1 << current)) != 0)
            {
                ray.coll = Collision(true, parseLeaf(octree[nextAddress], octree[nextAddress + 1]));
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

void main() {
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - ray.origin);
    ray.invDirection = 1.0 / ray.direction;
    ray.t = 0.0;
    ray.coll = Collision(false, parseLeaf(0, 0));

    traceRay(ray);

    if (ray.coll.hit)
    {
        outColor = vec4(ray.coll.voxel.normal / 2 + 0.5, 1.0);
    }
    else
    {
        outColor = vec4(0.1, 0.1, 0.1, 1.0);
    }
}