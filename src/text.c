#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gl.h"
#include "text.h"

static uint8_t font[];

static GLuint font_tex;

static const char *text_vertex_shader_src =
	"attribute vec2 a_coords;"
	"attribute vec2 a_uv;"
	"varying vec2 v_uv;"
	"uniform vec2 u_scale;"
	"uniform vec2 u_shift;"
	"void main() {"
		"gl_Position = vec4(a_coords * u_scale + u_shift, 0.0, 1.0);"
		"v_uv = a_uv;"
	"}";

static const char *text_fragment_shader_src =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"varying vec2 v_uv;"
	"uniform sampler2D u_tex;"
	"void main() {"
		"vec4 sample = texture2D(u_tex, v_uv);"
		"if (sample.a == 0.0)"
			"discard;"
		"gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
	"}";

GLuint text_program;
GLuint text_program_coord_attrib;
GLuint text_program_uv_attrib;
GLuint text_program_scale_uniform;
GLuint text_program_shift_uniform;
GLuint text_program_tex_uniform;

static void make_font_tex_letter(uint8_t *buf, int stride, uint8_t *letter)
{
	uint8_t c;
	int x;
	int y;

	for (y = 0; y < 8; y++) {
		c = letter[7 - y];
		for (x = 0; x < 8; x++)
			buf[x] = (c >> (7 - x)) & 1;
		buf += stride;
	}
}

static void make_font_tex(void)
{
	uint8_t *data;
	int x;
	int y;

	data = malloc(128 * 128);

	memset(data, 0, 128 * 128);

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 16; x++)
			make_font_tex_letter(data + x * 8 + y * 1024, 128, &font[(x + y * 16) * 8]);
	}

	glGenTextures(1, &font_tex);
	glBindTexture(GL_TEXTURE_2D, font_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 128, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	free(data);
}

bool text_compile_shaders(void)
{
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLint value;

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &text_vertex_shader_src, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &text_fragment_shader_src, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	text_program = glCreateProgram();
	glAttachShader(text_program, vertex_shader);
	glAttachShader(text_program, fragment_shader);
	glLinkProgram(text_program);
	glGetProgramiv(text_program, GL_LINK_STATUS, &value);
	if (!value)
		return false;

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	text_program_coord_attrib = glGetAttribLocation(text_program, "a_coords");
	text_program_uv_attrib = glGetAttribLocation(text_program, "a_uv");
	text_program_scale_uniform = glGetUniformLocation(text_program, "u_scale");
	text_program_shift_uniform = glGetUniformLocation(text_program, "u_shift");
	text_program_tex_uniform = glGetUniformLocation(text_program, "u_tex");

	make_font_tex();

	return true;
} 

static float _text_scale = FONT_SCALE_DEFAULT;

void text_set_scale(float scale) {
	_text_scale = scale;
}

static void make_letter_verts(float *data, uint8_t c, int idx)
{
	static const int xi[6] = { 0, 1, 1, 0, 1, 0 };
	static const int yi[6] = { 0, 0, 1, 0, 1, 1 };
	float x[2], y[2], s[2], t[2];
	int i;

	x[0] = _text_scale * idx * FONT_X_INCREMENT;
	x[1] = x[0] + _text_scale * 5.0f;

	y[0] = 0.0f;
	y[1] = _text_scale * 8.0f;

	s[0] = (c & 0xf) * 0.0625f;
	s[1] = s[0] + (5.0f / 128.0f);

	t[0] = (c >> 4) * 0.0625f;
	t[1] = t[0] + 0.0625f;

	for (i = 0; i < 6; i++) {
		data[i * 4 + 0] = x[xi[i]];
		data[i * 4 + 1] = y[yi[i]];
		data[i * 4 + 2] = s[xi[i]];
		data[i * 4 + 3] = t[yi[i]];
	}
}

void text_create(struct text *text, char *str)
{
	float *data;
	size_t len;
	size_t i;

	len = strlen(str);
	data = calloc(len * 24, sizeof(float));

	for (i = 0; i < len; i++)
		make_letter_verts(&data[i * 24], str[i], i);

	glGenBuffers(1, &text->vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, len * 24 * 4, data, GL_STATIC_DRAW);
	text->vertex_cnt = len * 6;

	free(data);
}

void text_render(struct text *text, int w, int h, int x, int y)
{
	glUseProgram(text_program);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buf);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void *)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void *)8);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font_tex);
	glUniform1i(text_program_tex_uniform, 0);

	glUniform2f(text_program_scale_uniform, 2.0f / w, 2.0f / h);
	glUniform2f(text_program_shift_uniform,
		    (float)(x * 2 - w) / w,
		    (float)(y * 2 - h) / h);

	glDrawArrays(GL_TRIANGLES, 0, text->vertex_cnt);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

void text_stream_create(struct text_stream *text, size_t len)
{
	glGenBuffers(1, &text->vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, len * 24 * 4, NULL, GL_STREAM_DRAW);
	text->vertex_cnt = 0;
	text->len = len;
}

void text_stream_update(struct text_stream *text, const char *str)
{
	float *data;
	size_t len;
	size_t i;

	len = strlen(str);
	if (len > text->len)
		len = text->len;

	data = calloc(len * 24, sizeof(float));

	for (i = 0; i < len; i++)
		make_letter_verts(&data[i * 24], str[i], i);

	glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buf);
	glBufferSubData(GL_ARRAY_BUFFER, 0, len * 24 * 4, data);
	text->vertex_cnt = len * 6;

	free(data);
}

void text_stream_render(struct text_stream *text, int w, int h, int x, int y)
{
	struct text t;

	t.vertex_buf = text->vertex_buf;
	t.vertex_cnt = text->vertex_cnt;

	text_render(&t, w, h, x, y);
}

static uint8_t font[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 01 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 02 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 03 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 04 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 05 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 06 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 08 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 09 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0a */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0b */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0c */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0d */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0e */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 10 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 11 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 12 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 13 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 14 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 15 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 16 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 18 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 19 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1a */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1b */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1c */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1d */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1e */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 20 - space */
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x20, 0x00, /* 21 - ! */
	0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 22 - " */
	0x00, 0x50, 0xf8, 0x50, 0xf8, 0x50, 0x00, 0x00, /* 23 - # */
	0x20, 0x70, 0x80, 0x70, 0x08, 0x70, 0x20, 0x00, /* 24 - $ */
	0x00, 0x88, 0x10, 0x20, 0x40, 0x88, 0x00, 0x00, /* 25 - % */
	0x60, 0x90, 0xa0, 0x40, 0xa8, 0x90, 0x68, 0x00, /* 26 - & */
	0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 27 - ' */
	0x10, 0x20, 0x40, 0x40, 0x40, 0x20, 0x10, 0x00, /* 28 - ( */
	0x40, 0x20, 0x10, 0x10, 0x10, 0x20, 0x40, 0x00, /* 29 - ) */
	0x00, 0x00, 0x50, 0x20, 0x50, 0x00, 0x00, 0x00, /* 2a - * */
	0x00, 0x20, 0x20, 0xf8, 0x20, 0x20, 0x00, 0x00, /* 2b - + */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x60, /* 2c - , */
	0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, /* 2d - - */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, /* 2e - . */
	0x08, 0x10, 0x10, 0x20, 0x40, 0x40, 0x80, 0x00, /* 2f - / */
	0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70, 0x00, /* 30 - 0 */
	0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0xf8, 0x00, /* 31 - 1 */
	0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xf8, 0x00, /* 32 - 2 */
	0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70, 0x00, /* 33 - 3 */
	0x10, 0x30, 0x50, 0x90, 0xf0, 0x10, 0x10, 0x00, /* 34 - 4 */
	0xf8, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70, 0x00, /* 35 - 5 */
	0x70, 0x88, 0x80, 0xf0, 0x88, 0x88, 0x70, 0x00, /* 36 - 6 */
	0xf8, 0x08, 0x10, 0x20, 0x20, 0x40, 0x40, 0x00, /* 37 - 7 */
	0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00, /* 38 - 8 */
	0x70, 0x88, 0x88, 0x78, 0x08, 0x88, 0x70, 0x00, /* 39 - 9 */
	0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, /* 3a - : */
	0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x60, /* 3b - ; */
	0x00, 0x10, 0x20, 0x40, 0x20, 0x10, 0x00, 0x00, /* 3c - < */
	0x00, 0x00, 0xf8, 0x00, 0xf8, 0x00, 0x00, 0x00, /* 3d - = */
	0x00, 0x40, 0x20, 0x10, 0x20, 0x40, 0x00, 0x00, /* 3e - > */
	0x70, 0x08, 0x08, 0x10, 0x20, 0x00, 0x20, 0x00, /* 3f - ? */
	0x00, 0x00, 0x70, 0x08, 0x68, 0xa8, 0xf0, 0x00, /* 40 - @ */
	0x70, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88, 0x00, /* 41 - A */
	0xf0, 0x88, 0x88, 0xf0, 0x88, 0x88, 0xf0, 0x00, /* 42 - B */
	0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0x00, /* 43 - C */
	0xf0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xf0, 0x00, /* 44 - D */
	0xf8, 0x80, 0x80, 0xe0, 0x80, 0x80, 0xf8, 0x00, /* 45 - E */
	0xf8, 0x80, 0x80, 0xe0, 0x80, 0x80, 0x80, 0x00, /* 46 - F */
	0x70, 0x88, 0x80, 0xb8, 0x88, 0x88, 0x70, 0x00, /* 47 - G */
	0x88, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88, 0x00, /* 48 - H */
	0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00, /* 49 - I */
	0x70, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60, 0x00, /* 4a - J */
	0x88, 0x90, 0xa0, 0xc0, 0xa0, 0x90, 0x88, 0x00, /* 4b - K */
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xf8, 0x00, /* 4c - L */
	0x88, 0xd8, 0xa8, 0xa8, 0x88, 0x88, 0x88, 0x00, /* 4d - M */
	0x88, 0xc8, 0xc8, 0xa8, 0x98, 0x98, 0x88, 0x00, /* 4e - N */
	0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00, /* 4f - O */
	0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80, 0x00, /* 50 - P */
	0x70, 0x88, 0x88, 0x88, 0xa8, 0x98, 0x78, 0x00, /* 51 - Q */
	0xf0, 0x88, 0x88, 0xf0, 0xa0, 0x90, 0x88, 0x00, /* 52 - R */
	0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00, /* 53 - S */
	0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, /* 54 - T */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00, /* 55 - U */
	0x88, 0x88, 0x88, 0x50, 0x50, 0x50, 0x20, 0x00, /* 56 - V */
	0x88, 0x88, 0x88, 0xa8, 0xa8, 0xa8, 0x50, 0x00, /* 57 - W */
	0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88, 0x00, /* 58 - X */
	0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20, 0x00, /* 59 - Y */
	0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8, 0x00, /* 5a - Z */
	0x70, 0x40, 0x40, 0x40, 0x40, 0x40, 0x70, 0x00, /* 5b - [ */
	0x80, 0x40, 0x40, 0x20, 0x10, 0x10, 0x08, 0x00, /* 5c - \ */
	0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x70, 0x00, /* 5d - ] */
	0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, /* 5e - ^ */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, /* 5f - _ */
	0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 60 - ` */
	0x00, 0x00, 0x70, 0x08, 0x78, 0x88, 0x78, 0x00, /* 61 - a */
	0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0xf0, 0x00, /* 62 - b */
	0x00, 0x00, 0x70, 0x88, 0x80, 0x88, 0x70, 0x00, /* 63 - c */
	0x08, 0x08, 0x78, 0x88, 0x88, 0x88, 0x78, 0x00, /* 64 - d */
	0x00, 0x00, 0x70, 0x88, 0xf0, 0x80, 0x70, 0x00, /* 65 - e */
	0x30, 0x48, 0x40, 0xe0, 0x40, 0x40, 0x40, 0x00, /* 66 - f */
	0x00, 0x00, 0x78, 0x88, 0x88, 0x78, 0x08, 0x70, /* 67 - g */
	0x80, 0x80, 0xb0, 0xc8, 0x88, 0x88, 0x88, 0x00, /* 68 - h */
	0x20, 0x00, 0x60, 0x20, 0x20, 0x20, 0x70, 0x00, /* 69 - i */
	0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x90, 0x60, /* 6a - j */
	0x80, 0x80, 0x88, 0xb0, 0xc0, 0xa0, 0x98, 0x00, /* 6b - k */
	0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x00, /* 6c - l */
	0x00, 0x00, 0xd0, 0xa8, 0xa8, 0xa8, 0xa8, 0x00, /* 6d - m */
	0x00, 0x00, 0xf0, 0x88, 0x88, 0x88, 0x88, 0x00, /* 6e - n */
	0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00, /* 6f - o */
	0x00, 0x00, 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, /* 70 - p */
	0x00, 0x00, 0x78, 0x88, 0x88, 0x78, 0x08, 0x08, /* 71 - q */
	0x00, 0x00, 0xb0, 0xc8, 0x80, 0x80, 0x80, 0x00, /* 72 - r */
	0x00, 0x00, 0x70, 0x80, 0x70, 0x08, 0x70, 0x00, /* 73 - s */
	0x40, 0x40, 0xe0, 0x40, 0x40, 0x48, 0x30, 0x00, /* 74 - t */
	0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x78, 0x00, /* 75 - u */
	0x00, 0x00, 0x88, 0x88, 0x50, 0x50, 0x20, 0x00, /* 76 - v */
	0x00, 0x00, 0x88, 0x88, 0xa8, 0xa8, 0x58, 0x00, /* 77 - w */
	0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88, 0x00, /* 78 - x */
	0x00, 0x00, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70, /* 79 - y */
	0x00, 0x00, 0xf8, 0x10, 0x20, 0x40, 0xf8, 0x00, /* 7a - z */
	0x30, 0x40, 0x40, 0x80, 0x40, 0x40, 0x30, 0x00, /* 7b - { */
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, /* 7c - | */
	0x60, 0x10, 0x10, 0x08, 0x10, 0x10, 0x60, 0x00, /* 7d - } */
	0x00, 0x00, 0x00, 0x68, 0xb0, 0x00, 0x00, 0x00, /* 7e - ~ */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 7f */
};
