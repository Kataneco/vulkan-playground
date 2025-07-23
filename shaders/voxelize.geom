#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 normals[];
layout(location = 1) in vec2 texCoords[];

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTexCoord_outPriority;
layout(location = 3) out vec3 outTreeRoot;

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: root grid dimensions^3
} data;

float priority(float size) {
    return -(1/(size+1))+1;
}

vec3 posToRoot(vec3 pos) {
    return floor(pos/data.resolution.y);
}

vec3 rootOrigin(vec3 root) {
    return (root+0.5)*data.resolution.y;
}

vec3 rootBegin(vec3 r) {
    return r*data.resolution.y;
}

void main() {
    vec3 u = gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    vec3 v = gl_in[2].gl_Position.xyz-gl_in[0].gl_Position.xyz;
    vec3 w = gl_in[2].gl_Position.xyz-gl_in[1].gl_Position.xyz;
    vec3 facenormal = abs(cross(u, v));

    uint maxi = facenormal[1] > facenormal[0] ? 1 : 0;
    maxi = facenormal[2] > facenormal[maxi] ? 2 : maxi;

    vec3 ra, rb, rc;
    ra=posToRoot(gl_in[0].gl_Position.xyz);
    rb=posToRoot(gl_in[1].gl_Position.xyz);
    rc=posToRoot(gl_in[2].gl_Position.xyz);

    vec3 minRoot = min(min(ra, rb), rc);
    minRoot = clamp(minRoot, vec3(-data.resolution.z*0.5), vec3(data.resolution.z*0.5-1.0));
    vec3 maxRoot = max(max(ra, rb), rc);
    maxRoot = clamp(maxRoot, vec3(-data.resolution.z*0.5), vec3(data.resolution.z*0.5-1.0));
    
    for(float z = minRoot.z; z <= maxRoot.z; z+=1.0)
    for(float y = minRoot.y; y <= maxRoot.y; y+=1.0)
    for(float x = minRoot.x; x <= maxRoot.x; x+=1.0) {
    for (uint i = 0; i < 3; i++) {
        vec3 root = vec3(x,y,z);
        gl_Position = vec4(((gl_in[i].gl_Position.xyz)-rootOrigin(root))/(data.resolution.y/2), 1.0);

        if (maxi == 0) {
            gl_Position.xyz = gl_Position.zyx;
        }
        else if (maxi == 1) {
            gl_Position.xyz = gl_Position.xzy;
        }

        gl_Position.z = abs(gl_Position.z);
        //gl_Position.z = 1.0f;
        outPosition = ((gl_in[i].gl_Position.xyz-rootBegin(vec3(0)))+(vec3(data.resolution.y*0.5*data.resolution.z)))*(data.resolution.x*data.resolution.z-1);
        //outNormal = normals[i];
        outNormal = facenormal;
        outTexCoord_outPriority = vec3(texCoords[i], priority((length(u)+1+length(v)+1)*(length(w)+1)));
        outTreeRoot = root;
        EmitVertex();
    }
    EndPrimitive();
    }
}