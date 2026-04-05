typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

extern "C" GLuint glCreateBuffer(void);

extern "C" void glGenBuffers(GLsizei n, GLuint *buffers) {
  for (GLsizei i = 0; i < n; i++)
    buffers[i] = glCreateBuffer();
}

extern "C" GLuint glCreateTexture(void);

extern "C" void glGenTextures(GLsizei n, GLuint *textures) {
  for (GLsizei i = 0; i < n; i++)
    textures[i] = glCreateTexture();
}

extern "C" GLint glGetProgramParameter(GLuint program, GLenum pname);

extern "C" void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
  *params = glGetProgramParameter(program, pname);
}

extern "C" GLint glGetShaderParameter(GLuint shader, GLenum pname);

extern "C" void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
  *params = glGetShaderParameter(shader, pname);
}
