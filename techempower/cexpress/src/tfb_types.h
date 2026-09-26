#ifndef TFB_TYPES_H
#define TFB_TYPES_H

#include <stddef.h>

/* Limits fixed by the TechEmpower spec. */
#define WORLD_ROWS 10000          /* World ids are 1..10000 */
#define MAX_QUERIES 500           /* ?queries= is clamped to 1..500 */
#define MAX_FORTUNES 64           /* the Fortune table has 12 rows; one more is added per request */

typedef struct {
    int id;
    int random_number;
} World;

/* `message` points into a PGresult (or a string literal) owned by the handler's frame. */
typedef struct {
    int id;
    const char *message;
    size_t message_len;
} Fortune;

#endif
