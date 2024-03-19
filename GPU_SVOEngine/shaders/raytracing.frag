#version 450

layout ( push_constant ) uniform PushConstants {
	vec4 camPos;
    mat4 invPVMatrix;
};

layout(location = 0) in vec2 fragScreenCoord;

layout(location = 0) out vec4 outColor;

struct Sphere {
    vec3 origin;
    float radius;
    vec3 color;
};

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

Collision hitSphere(Ray ray, Sphere sphere){
    float a = dot(ray.direction, ray.direction);
    vec3 sphereToRayPos = ray.origin - sphere.origin;
    float b = 2.0 * dot(ray.direction, sphereToRayPos);
    float c = dot(sphereToRayPos, sphereToRayPos) - (sphere.radius * sphere.radius);
    if (b * b - 4.0 * a * c < 0.0)
        return Collision(false, 0, vec3(0.0, 0.0, 0.0));
    float root = sqrt(b*b - 4.0 * a * c);
    float pt = (-b - root) / (2.0 * a);
    float nt = (-b + root) / (2.0 * a);
    float t = min(pt, nt);
    if (t < 0) t = max(pt, nt);
    if (t < 0) return Collision(false, 0, vec3(0.0, 0.0, 0.0));
    return Collision(true, t, normalize((ray.origin + ray.direction * t) - sphere.origin));
}

vec3 homogenize(vec4 p){
    return p.xyz / p.w;
}

void main() {
    Sphere sphere[] = {
        Sphere(vec3(0.0, -1.0, 5.0), 3.0, vec3(0.8, 0.2, 0.1)),
        Sphere(vec3(2.0, 3.0, -2.0), 2.0, vec3(0.1, 0.6, 0.4)),
        Sphere(vec3(4.0, -2.0, 2.0), 3.0, vec3(0.5, 0.3, 0.2)),
        Sphere(vec3(-3.0, 5.0, 0.0), 0.5, vec3(0.3, 0.8, 0.8)),
        Sphere(vec3(-2.0, 1.0, 2.0), 1.5, vec3(1.0, 0.1, 0.5)),
        Sphere(vec3(0.0, 0.0, 0.0), 1.0, vec3(1.0, 1.0, 1.0))
    };
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - camPos.xyz);
    ray.t = 0;

    vec3 lastNormal = vec3(0.0, 0.0, 0.0);
    vec3 lastColor = vec3(0.0, 0.0, 0.0);
    for (uint i = 0; i < 6; ++i){
        Collision coll = hitSphere(ray, sphere[i]);
        if (coll.hit && (ray.t == 0 || coll.t < ray.t)){
            ray.t = coll.t;
            lastNormal = coll.normal;
            lastColor = sphere[i].color;
        }
    }

    if (ray.t == 0)
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
    else{
        vec3 diffuseFinal = lastColor * clamp(dot(vec3(1.0, 1.0, -1.0), normalize(lastNormal)), 0, 1);
		outColor = vec4(lastColor * 0.05 + diffuseFinal, 1.0);
        //outColor = vec4(lastNormal / 2.0 + 0.5, 1.0);
    }
}