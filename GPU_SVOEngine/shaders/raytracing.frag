#version 450

layout ( push_constant ) uniform PushConstants {
	vec4 camPos;
    mat4 invPVMatrix;
};

layout(binding = 0) buffer MyBuffer {
  uint octree[];
};

layout(location = 0) in vec2 fragScreenCoord;

layout(location = 0) out vec4 outColor;

struct Ray{
    vec3 origin;
    vec3 direction;
    float t;
};

struct Collision {
    bool hit;
	float t;
	vec3 normal;
};

struct BranchNode {
    uint address;
    uint farFlag;
    uint childMask;
    uint leafMask;
};

struct LeafNode {
    uint material;
    vec3 normal;
    vec3 color;
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

uint orders[][] = {
    {7, 6, 5, 3, 4, 2, 1, 0},
    {6, 7, 2, 4, 5, 3, 0, 1},
    {5, 1, 7, 4, 3, 6, 0, 2},
    {4, 5, 6, 0, 7, 2, 1, 3},
	{3, 2, 7, 1, 0, 6, 5, 4},
	{2, 6, 3, 0, 4, 7, 1, 5},
	{1, 0, 3, 5, 4, 2, 7, 6},
	{0, 4, 2, 1, 6, 3, 5, 7}
};


BranchNode parseBranch(uint node){
	BranchNode n;
    n.farFlag =   (node & 0x80000000) >> 31;
    n.address =   (node & 0x7FFF0000) >> 16;
    n.childMask = (node & 0x0000FF00) >> 16;
    n.leafMask =  (node & 0x000000FF);
	return n;
}

LeafNode parseLeaf(uint node){
	LeafNode n;
	n.material = (node & 0xFF000000) >> 24;
    n.normal.x = (node & 0x00F00000) >> 20;
    n.normal.y = (node & 0x000F0000) >> 16;
    n.normal.z = (node & 0x0000F000) >> 12;
    n.color.x =  (node & 0x00000F00) >> 8;
    n.color.y =  (node & 0x000000F0) >> 4;
    n.color.z =  (node & 0x0000000F);
	return n;
}

vec3 homogenize(vec4 p){
    return p.xyz / p.w;
}

void main() {
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - ray.origin);
    ray.t = 0;



    outColor = vec4(0.1, 0.1, 0.1, 1.0);
}