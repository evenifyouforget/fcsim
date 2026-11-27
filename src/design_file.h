#ifndef DESIGN_FILE_H
#define DESIGN_FILE_H

/* Binary design file (#179). Format: README §Design File. */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct design;
struct xml_level;

/* true if buf starts with the design file magic */
bool design_file_is_binary(const char *buf, int len);

/* Encode a design. Returns a malloc'd buffer and sets *len. */
uint8_t *design_file_encode(struct design *design, int *len);

/* Decode into level, for convert_xml. Returns 0 on success. On failure
 * level is left empty (no blocks, level_id -1). Free with xml_free. */
int design_file_decode(const uint8_t *buf, int len, struct xml_level *level);

#ifdef __cplusplus
}
#endif

#endif
