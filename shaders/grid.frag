#version 460

layout (push_constant) uniform Camera {
    vec4 position;
    vec4 data; //xy, near, far
    mat4 inverse_projection;
    mat4 inverse_view;
} camera;

layout(location = 0) out vec4 outColor;

vec3 ray_direction(float x, float y) {
    vec2 coord = vec2(x/camera.data.x, y/camera.data.y);
    coord = coord * 2 - 1;

    vec4 target = camera.inverse_projection * vec4(coord.xy,1,1);
    return vec3(camera.inverse_view*vec4(normalize(target.xyz/target.w), 0));
}

bool ray_hit(vec3 normal, vec3 direction, out float t) {
    const float denominator = dot(normal, direction);
    if (abs(denominator) > 0.00001) {
        t = -dot(normal, camera.position.xyz) / denominator;
        return (t >= 0);
    }
    return false;
}

const float thickness = 0.0142f;

float linearDepthToNonLinear(float linearDepth, float near, float far) {
    return (far + near - 2.0 * near * far / linearDepth) / (far - near);
}

void main() {
    //vec3 D = normalize(cross(camera.direction.xyz, vec3(0,0,1)));
    //float a = dot(camera.direction.xyz, vec3(0,0,1));

    vec3 d = ray_direction(gl_FragCoord.x, gl_FragCoord.y);
    float t = 0.0f;

    outColor = vec4(0, 0, 0, 1);
    gl_FragDepth = 1.0f;

    if (d.z != 0) {
        t = -camera.position.z / d.z;
        if (t > 0) {
            vec3 pos = camera.position.xyz + (d*t);
            if (abs(fract(pos.x)) < thickness || abs(fract(pos.y)) < thickness) {
                outColor.x = 1;
                gl_FragDepth = linearDepthToNonLinear(t, camera.data.z, camera.data.w);
                //gl_FragDepth = 0.0f;
            }
        }
    }

    if (d.y != 0) {
        t = -camera.position.y / d.y;
        if (t > 0) {
            vec3 pos = camera.position.xyz+(d*t);
            if (abs(fract(pos.x)) < thickness || abs(fract(pos.z)) < thickness) {
                outColor.y = 1;
                gl_FragDepth = linearDepthToNonLinear(t, camera.data.z, camera.data.w);
                //gl_FragDepth = 0.0f;
            }
        }
    }

    if (d.x != 0) {
        t = -camera.position.x / d.x;
        if (t > 0) {
            vec3 pos = camera.position.xyz+(d*t);
            if (abs(fract(pos.z)) < thickness || abs(fract(pos.y)) < thickness) {
                outColor.z = 1;
                gl_FragDepth = linearDepthToNonLinear(t, camera.data.z, camera.data.w);
                //gl_FragDepth = 0.0f;
            }
        }
    }
}