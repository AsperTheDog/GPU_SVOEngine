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
};

struct Ray{
    vec3 origin;
    vec3 direction;
    float t;
};

vec3 hitSphere(Ray ray, Sphere sphere){
    float a = dot(ray.direction, ray.direction);
    vec3 sphereToRayPos = ray.origin - sphere.origin;
    float b = 2.0 * dot(ray.direction, sphereToRayPos);
    float c = dot(sphereToRayPos, sphereToRayPos) - (sphere.radius * sphere.radius);
    if (b * b - 4.0 * a * c < 0.0)
        return vec3(0.0, 0.0, 0.0);
    float root = sqrt(b*b - 4.0 * a * c);
    float pt = (-b - root) / (2.0 * a);
    float nt = (-b + root) / (2.0 * a);
    ray.t = min(pt, nt);
    return normalize((ray.origin + ray.direction * ray.t) - sphere.origin);
}

vec3 homogenize(vec4 p){
    return p.xyz / p.w;
}

void main() {
    Sphere sphere = {vec3(0.0, 0.0, 0.0), 1.0};
    Ray ray;
    ray.origin = camPos.xyz;
    ray.direction = normalize(homogenize(invPVMatrix * vec4(fragScreenCoord, 1.0, 1.0)) - camPos.xyz);

    vec3 normal = hitSphere(ray, sphere);

    if (normal == vec3(0.0, 0.0, 0.0))
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
    else{
        vec3 diffuseFinal = vec3(0.0, 0.0, 1.0) * clamp(dot(vec3(1.0, 1.0, -1.0), normalize(normal)) * 1.0, 0, 1);
		outColor = vec4(vec3(0.0, 0.0, 0.05) + diffuseFinal, 1.0);
    }
}