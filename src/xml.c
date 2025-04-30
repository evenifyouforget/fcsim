#include <stdlib.h>
#include <string.h>
#include <fpmath/fpmath.h>

#include "xml.h"
#include "str.h"

struct slice {
	char *ptr;
	long len;
};

static int strtoi(const char *str, int len, int *res)
{
	int val = 0;
	int neg = 0;
	int digit;
	int i;

	if (len == 0)
		return -1;
	if (*str == '+' || *str == '-') {
		if (*str == '-')
			neg = 1;
		str++;
		len--;
	}
	for (i = 0; i < len; i++) {
		digit = str[i] - '0';
		if (digit < 0 || digit > 10)
			return -1;
		val = val * 10 + digit;
	}
	*res = neg ? -val : val;

	return 0;
}

int is_ws(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int is_letter(char c)
{
	return 'a' <= c && c <= 'z' || 'A' <= c && c <= 'Z';
}

int slice_equal(struct slice *s1, struct slice *s2)
{
	return s1->len == s2->len && !memcmp(s1->ptr, s2->ptr, s1->len);
}

int slice_str_equal(struct slice *slice, const char *str)
{
	long len;

	len = strlen(str);

	return slice->len == len && !memcmp(slice->ptr, str, len);
}

void skip_ws(struct slice *buf)
{
	while (buf->len > 0 && is_ws(*buf->ptr)) {
		buf->ptr++;
		buf->len--;
	}
}

int read_char(struct slice *buf, char c)
{
	if (!buf->len)
		return -1;

	if (*buf->ptr != c)
		return -1;

	buf->ptr++;
	buf->len--;

	return 0;
}

int read_name(struct slice *buf, struct slice *name)
{
	long len = 0;

	while (len < buf->len && is_letter(buf->ptr[len]))
		len++;

	if (len == 0)
		return -1;

	name->ptr = buf->ptr;
	name->len = len;
	buf->ptr += len;
	buf->len -= len;

	return 0;
}

int read_value(struct slice *buf, struct slice *value)
{
	long len = 0;
	int res;

	res = read_char(buf, '"');
	if (res)
		return res;

	while (len < buf->len && buf->ptr[len] != '"')
		len++;

	value->ptr = buf->ptr;
	value->len = len;
	buf->ptr += len;
	buf->len -= len;

	res = read_char(buf, '"');
	if (res)
		return res;

	return 0;
}

int read_xml_decl(struct slice *buf)
{
	int res;

	res = read_char(buf, '<');
	if (res)
		return res;
	
	res = read_char(buf, '?');
	if (res)
		return res;

	while (buf->len > 0 && *buf->ptr != '?') {
		buf->ptr++;
		buf->len--;
	}
	
	res = read_char(buf, '?');
	if (res)
		return res;
	
	res = read_char(buf, '>');
	if (res)
		return res;

	return 0;
}

int read_char_data(struct slice *buf, struct slice *data)
{
	long len = 0;

	skip_ws(buf);

	data->ptr = buf->ptr;

	while (len < buf->len && buf->ptr[len] != '<')
		len++;

	buf->ptr += len;
	buf->len -= len;

	while (len && is_ws(data->ptr[len-1]))
		len--;

	data->len = len;

	return 0;
}

int peek_elem_name(struct slice *buf, struct slice *name)
{
	struct slice copy = *buf;
	int res;

	res = read_char(&copy, '<');
	if (res)
		return 0;

	res = read_name(&copy, name);
	if (res)
		return 0;

	return 1;
}

int read_elem_start_pre(struct slice *buf, struct slice *name)
{
	int res;

	res = read_char(buf, '<');
	if (res)
		return res;

	res = read_name(buf, name);
	if (res)
		return res;

	return 0;
}

int read_attr(struct slice *buf, struct slice *name, struct slice *value)
{
	int res;

	res = read_name(buf, name);
	if (res)
		return res;

	skip_ws(buf);

	res = read_char(buf, '=');
	if (res)
		return res;

	skip_ws(buf);

	res = read_value(buf, value);
	if (res)
		return res;

	return 0;
}

int read_elem_start_post(struct slice *buf, int *empty)
{
	int res;

	res = read_char(buf, '/');
	*empty = !res;

	res = read_char(buf, '>');
	if (res)
		return res;

	return 0;
}

int read_elem_start(struct slice *buf, struct slice *name, int *empty)
{
	struct slice attr_name;
	struct slice attr_value;
	int res;

	res = read_elem_start_pre(buf, name);
	if (res)
		return res;
		
	skip_ws(buf);

	while (buf->len && is_letter(*buf->ptr)) {
		res = read_attr(buf, &attr_name, &attr_value);
		if (res)
			return res;

		skip_ws(buf);
	}

	res = read_elem_start_post(buf, empty);
	if (res)
		return res;

	return 0;
}

int read_elem_end(struct slice *buf, struct slice *name)
{
	struct slice this_name;
	int res;

	res = read_char(buf, '<');
	if (res)
		return res;

	res = read_char(buf, '/');
	if (res)
		return res;

	res = read_name(buf, &this_name);
	if (res)
		return res;

	if (!slice_equal(&this_name, name))
		return -1;

	skip_ws(buf);

	res = read_char(buf, '>');
	if (res)
		return res;

	return 0;
}

int read_data_elem(struct slice *buf, struct slice *data)
{
	struct slice this_name;
	int empty;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty) {
		data->ptr = NULL;
		data->len = 0;
		return 0;
	}

	read_char_data(buf, data);

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	return 0;
}

int skip_data_elem(struct slice *buf)
{
	struct slice data;

	return read_data_elem(buf, &data);
}

int read_bool(struct slice *buf, int *val)
{
	struct slice data;
	int res;

	res = read_data_elem(buf, &data);
	if (res)
		return res;

	if (slice_str_equal(&data, "true"))
		*val = 1;
	else if (slice_str_equal(&data, "false"))
		*val = 0;
	else
		return -1;

	return 0;
}

int read_int(struct slice *buf, int *val)
{
	struct slice data;
	int res;

	res = read_data_elem(buf, &data);
	if (res)
		return res;

	res = strtoi(data.ptr, data.len, val);
	if (res)
		return res;

	return 0;
}

int read_number(struct slice *buf, double *val)
{
	struct slice data;
	int res;

	res = read_data_elem(buf, &data);
	if (res)
		return res;

	res = fp_strtod(data.ptr, data.len, val);
	if (res)
		return res;

	return 0;
}

int read_position(struct slice *buf, struct xml_position *position)
{
	struct slice this_name;
	int empty;
	struct slice name;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (slice_str_equal(&name, "x"))
			res = read_number(buf, &position->x);
		else if (slice_str_equal(&name, "y"))
			res = read_number(buf, &position->y);
		else
			res = skip_data_elem(buf);

		if (res)
			return res;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	return 0;
}

int read_joints(struct slice *buf, struct xml_joint **val)
{
	struct slice this_name;
	int empty;
	struct slice name;
	struct xml_joint *head = NULL;
	struct xml_joint *tail = NULL;
	struct xml_joint *joint;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (!slice_str_equal(&name, "jointedTo")) {
			res = skip_data_elem(buf);
			if (res)
				return res;
			skip_ws(buf);
		}

		joint = calloc(1, sizeof(*joint));
		res = read_int(buf, &joint->id);
		if (res)
			return res;

		if (tail)
			tail->next = joint;
		else
			head = joint;
		tail = joint;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	*val = head;

	return 0;
}

int block_type_from_name(struct slice *name)
{
	if (slice_str_equal(name, "StaticRectangle"))
		return XML_STATIC_RECTANGLE;
	if (slice_str_equal(name, "StaticCircle"))
		return XML_STATIC_CIRCLE;
	if (slice_str_equal(name, "DynamicRectangle"))
		return XML_DYNAMIC_RECTANGLE;
	if (slice_str_equal(name, "DynamicCircle"))
		return XML_DYNAMIC_CIRCLE;
	if (slice_str_equal(name, "JointedDynamicRectangle"))
		return XML_JOINTED_DYNAMIC_RECTANGLE;
	if (slice_str_equal(name, "NoSpinWheel"))
		return XML_NO_SPIN_WHEEL;
	if (slice_str_equal(name, "ClockwiseWheel"))
		return XML_CLOCKWISE_WHEEL;
	if (slice_str_equal(name, "CounterClockwiseWheel"))
		return XML_COUNTER_CLOCKWISE_WHEEL;
	if (slice_str_equal(name, "SolidRod"))
		return XML_SOLID_ROD;
	if (slice_str_equal(name, "HollowRod"))
		return XML_HOLLOW_ROD;
	return -1;
}

int read_block_attrs(struct slice *buf, struct xml_block *block)
{
	struct slice name;
	struct slice value;
	int res;

	block->id = -1;

	while (buf->len && is_letter(*buf->ptr)) {
		res = read_attr(buf, &name, &value);
		if (res)
			return res;

		if (slice_str_equal(&name, "id")) {
			res = strtoi(value.ptr, value.len, &block->id);
			if (res)
				return res;
		}

		skip_ws(buf);
	}

	return 0;
}

int read_block(struct slice *buf, struct xml_block *block)
{
	struct slice this_name;
	int empty;
	struct slice name;
	int res;

	res = read_elem_start_pre(buf, &this_name);
	if (res)
		return res;
	block->type = block_type_from_name(&this_name);

	skip_ws(buf);

	res = read_block_attrs(buf, block);
	if (res)
		return res;

	res = read_elem_start_post(buf, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (slice_str_equal(&name, "rotation"))
			res = read_number(buf, &block->rotation);
		else if (slice_str_equal(&name, "position"))
			res = read_position(buf, &block->position);
		else if (slice_str_equal(&name, "width"))
			res = read_number(buf, &block->width);
		else if (slice_str_equal(&name, "height"))
			res = read_number(buf, &block->height);
		else if (slice_str_equal(&name, "goalBlock"))
			res = read_bool(buf, &block->goal_block);
		else if (slice_str_equal(&name, "joints"))
			res = read_joints(buf, &block->joints);
		else
			res = skip_data_elem(buf);

		if (res)
			return res;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	return 0;
}

int read_block_list(struct slice *buf, struct xml_block **val)
{
	struct slice this_name;
	int empty;
	struct slice name;
	struct xml_block *head = NULL;
	struct xml_block *tail = NULL;
	struct xml_block *block;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		block = calloc(1, sizeof(*block));
		if (!block)
			return -1;

		res = read_block(buf, block);
		if (res)
			return res;

		if (tail)
			tail->next = block;
		else
			head = block;
		tail = block;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	*val = head;

	return 0;
}

int read_zone(struct slice *buf, struct xml_zone *zone)
{
	struct slice this_name;
	int empty;
	struct slice name;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (slice_str_equal(&name, "position"))
			res = read_position(buf, &zone->position);
		else if (slice_str_equal(&name, "width"))
			res = read_number(buf, &zone->width);
		else if (slice_str_equal(&name, "height"))
			res = read_number(buf, &zone->height);
		else
			res = skip_data_elem(buf);

		if (res)
			return res;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	return 0;
}

int read_level(struct slice *buf, struct xml_level *level)
{
	struct slice this_name;
	int empty;
	struct slice name;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (slice_str_equal(&name, "levelBlocks"))
			res = read_block_list(buf, &level->level_blocks);
		else if (slice_str_equal(&name, "playerBlocks"))
			res = read_block_list(buf, &level->player_blocks);
		else if (slice_str_equal(&name, "start"))
			res = read_zone(buf, &level->start);
		else if (slice_str_equal(&name, "end"))
			res = read_zone(buf, &level->end);
		else
			res = skip_data_elem(buf);

		if (res)
			return res;

		skip_ws(buf);
	}

	res = read_elem_end(buf, &this_name);
	if (res)
		return res;

	return 0;
}

int read_level_id(struct slice *buf, int *val)
{
	struct slice data;
	int res;

	res = read_data_elem(buf, &data);
	if (res)
		return res;

	/* some designs have an empty levelId */
	if (data.len == 0) {
		*val = -1;
		return 0;
	}

	res = strtoi(data.ptr, data.len, val);
	if (res)
		return res;

	return 0;
}


int read_retrieve_level(struct slice *buf, struct xml_level *level)
{
	struct slice this_name;
	int empty;
	struct slice name;
	int res;

	res = read_elem_start(buf, &this_name, &empty);
	if (res)
		return res;

	if (empty)
		return 0;

	skip_ws(buf);

	while (peek_elem_name(buf, &name)) {
		if (slice_str_equal(&name, "level"))
			res = read_level(buf, level);
		else if (slice_str_equal(&name, "levelId"))
			res = read_level_id(buf, &level->level_id);
		else
			res = skip_data_elem(buf);

		if (res)
			return res;

		skip_ws(buf);
	}

	return read_elem_end(buf, &this_name);
}

int xml_parse(char *xml, int len, struct xml_level *level)
{
	struct slice buf;
	int res;

	buf.ptr = xml;
	buf.len = len;

	skip_ws(&buf);

	res = read_xml_decl(&buf);
	if (res)
		return res;

	skip_ws(&buf);

	res = read_retrieve_level(&buf, level);
	if (res)
		return res;

	skip_ws(&buf);

	if (buf.len > 0)
		return -1;

	return 0;
}

static void free_joints(struct xml_joint *joint)
{
	struct xml_joint *next;

	while (joint) {
		next = joint->next;
		free(joint);
		joint = next;
	}
}

static void free_block(struct xml_block *block)
{
	free_joints(block->joints);
	free(block);
}

static void free_blocks(struct xml_block *block)
{
	struct xml_block *next;

	while (block) {
		next = block->next;
		free_block(block);
		block = next;
	}
}

void xml_free(struct xml_level *level)
{
	free_blocks(level->level_blocks);
	free_blocks(level->player_blocks);
}
