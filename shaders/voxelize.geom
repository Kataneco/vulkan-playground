#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 normals[];
layout(location = 1) in vec2 texCoords[];

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTexCoord_outPriority;

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: unassigned
} data;

float priority(float size) {
    return -(1/(size+1))+1;
}

void main() {
    vec3 u = gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    vec3 v = gl_in[2].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    vec3 w = gl_in[2].gl_Position.xyz-gl_in[1].gl_Position.xyz;
    vec3 facenormal = abs(cross(u, v));

    uint maxi = facenormal[1] > facenormal[0] ? 1 : 0;
    maxi = facenormal[2] > facenormal[maxi] ? 2 : maxi;

    for (uint i = 0; i < 3; i++) {
        gl_Position = vec4(((gl_in[i].gl_Position.xyz)-data.center)/(data.resolution.y/2), 1.0f);

        if (maxi == 0) {
            gl_Position.xyz = gl_Position.zyx;
        }
        else if (maxi == 1) {
            gl_Position.xyz = gl_Position.xzy;
        }

        gl_Position.z = abs(gl_Position.z);
        //gl_Position.z = 1.0f;
        outPosition = ((gl_in[i].gl_Position.xyz-data.center)+(vec3(data.resolution.y/2)))*data.resolution.x-1;
        //outNormal = normals[i];
        outNormal = facenormal;
        outTexCoord_outPriority = vec3(texCoords[i], priority((length(u)+1+length(v)+1)*(length(w)+1)));
        EmitVertex();
    }
    EndPrimitive();
}