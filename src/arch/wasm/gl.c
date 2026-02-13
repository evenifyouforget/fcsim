typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

GLuint glCreateBuffer(void);

void glGenBuffers(GLsizei n, GLuint *buffers) {
  GLsizei i;

  for (i = 0; i < n; i++)
    buffers[i] = glCreateBuffer();
}

GLuint glCreateTexture(void);

void glGenTextures(GLsizei n, GLuint *textures) {
  GLsizei i;

  for (i = 0; i < n; i++)
    textures[i] = glCreateTexture();
}

GLint glGetProgramParameter(GLuint program, GLenum pname);

void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
  *params = glGetProgramParameter(program, pname);
}

GLint glGetShaderParameter(GLuint shader, GLenum pname);

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
  *params = glGetShaderParameter(shader, pname);
}
