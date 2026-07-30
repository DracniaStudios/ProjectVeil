#version 330

// Veil forward lighting — vertex stage
//
// Every input below is supplied by raylib automatically. LoadShader() resolves
// the attribute names ("vertexPosition", "vertexTexCoord", "vertexNormal",
// "vertexColor") and the matrix uniforms ("mvp", "matModel", "matNormal") by
// name, and DrawMesh() uploads them per draw call — so this stage needs no
// help from LightingSystem at all.

// Vertex attributes (bound to fixed locations by raylib before linking)
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Matrices raylib fills in for every DrawMesh() call
uniform mat4 mvp;       // model-view-projection
uniform mat4 matModel;  // model transform, including GameObject::render3D's baked scale+rotation
uniform mat4 matNormal; // transpose(inverse(matModel)) — world-space normal matrix

// Interpolated outputs consumed by lighting.fs
out vec3 fragPosition;  // world space
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;    // world space

void main()
{
    // World-space position is what every light needs: point/spot lights measure
    // distance to it, the shadow lookup reprojects it, and fog measures the
    // camera's distance from it.
    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));

    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // matNormal is the inverse-transpose, so non-uniform rigidBody3D.scale
    // (baked into model.transform by GameObject::render3D) does not skew the
    // normal. Meshes without a normal attribute get raylib's (1,1,1) default,
    // which normalize() keeps finite.
    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 0.0)));

    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
