#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "button.h"
#include "gl.h"

static const char *button_vertex_shader_src =
    "attribute vec2 a_coords;"
    "uniform vec2 u_scale;"
    "uniform vec2 u_shift;"
    "void main() {"
    "gl_Position = vec4(a_coords * u_scale + u_shift, 0.0, 1.0);"
    "}";

static const char *button_fragment_shader_src =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform vec3 u_color;"
    "void main() {"
    "gl_FragColor = vec4(u_color, 1.0);"
    "}";

GLuint button_program;
GLuint button_program_coord_attrib;
GLuint button_program_scale_uniform;
GLuint button_program_shift_uniform;
GLuint button_program_color_uniform;

bool button_compile_shaders(void) {
  GLuint vertex_shader;
  GLuint fragment_shader;
  GLint value;

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &button_vertex_shader_src, NULL);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &value);
  if (!value)
    return false;

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &button_fragment_shader_src, NULL);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &value);
  if (!value)
    return false;

  button_program = glCreateProgram();
  glAttachShader(button_program, vertex_shader);
  glAttachShader(button_program, fragment_shader);
  glLinkProgram(button_program);
  glGetProgramiv(button_program, GL_LINK_STATUS, &value);
  if (!value)
    return false;

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  button_program_coord_attrib = glGetAttribLocation(button_program, "a_coords");
  button_program_scale_uniform =
      glGetUniformLocation(button_program, "u_scale");
  button_program_shift_uniform =
      glGetUniformLocation(button_program, "u_shift");
  button_program_color_uniform =
      glGetUniformLocation(button_program, "u_color");

  return true;
}

void button_create(struct button *button, int w, int h, float r, float g,
                   float b) {
  float data[8] = {
      0, 0, w, 0, w, h, 0, h,
  };

  glGenBuffers(1, &button->vertex_buf);
  glBindBuffer(GL_ARRAY_BUFFER, button->vertex_buf);
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
  button->r = r;
  button->g = g;
  button->b = b;
}

void button_render(struct button *button, int w, int h, int x, int y) {
  glUseProgram(button_program);

  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, button->vertex_buf);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

  glUniform2f(button_program_scale_uniform, 2.0f / w, 2.0f / h);
  glUniform2f(button_program_shift_uniform, (float)(x * 2 - w) / w,
              (float)(y * 2 - h) / h);
  glUniform3f(button_program_color_uniform, button->r, button->g, button->b);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glDisableVertexAttribArray(0);
}
