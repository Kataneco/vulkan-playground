#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 normals[];
layout(location = 1) in vec2 texCoords[];

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: unassigned
} data;

void main() {
    //vec3 facenormal = abs(vec3(normals[0] + normals[1] + normals[2]));

    vec3 u = gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    vec3 v = gl_in[2].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    //vec3 w = gl_in[2].gl_Position.xyz-gl_in[1].gl_Position.xyz;
    vec3 facenormal = abs(cross(u, v));
/*
    vec3 offsets[3];
    offsets[0] = -u-v;
    offsets[1] = u-w;
    offsets[2] = v+w;
*/
    uint maxi = facenormal[1] > facenormal[0] ? 1 : 0;
    maxi = facenormal[2] > facenormal[maxi] ? 2 : maxi;

    for (uint i = 0; i < 3; i++) {
        gl_Position = vec4((gl_in[i].gl_Position.xyz/*-data.center*/)/*+offsets[i]*0*/, 1);

        if (maxi == 0) {
            gl_Position.xyz = gl_Position.zyx;
        }
        else if (maxi == 1) {
            gl_Position.xyz = gl_Position.xzy;
        }

        //gl_Position.xy *= data.resolution.xy;
        gl_Position.z = 1.0f;
        outPosition = gl_in[i].gl_Position.xyz*data.resolution.x;
        outNormal = normals[i];
        outTexCoord = texCoords[i];
        EmitVertex();
    }
    EndPrimitive();
}