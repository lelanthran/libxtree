#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include "ds_str.h"

#include "xtree_err.h"
#include "xtree_reader.h"

struct xtree_reader_t {
  // Inputs provided by caller
  char *name;
  char *str_input;
  size_t str_len;

  // Track state of reader
  struct xtree_errobj_t err;
  size_t line;
  size_t cpos;
  size_t last_cpos;

  // Track state of input
  size_t str_index;
};


static xtree_reader_t *xtree_reader_new (const char *name)
{
  xtree_reader_t *ret = calloc (1, sizeof *ret);
  if (!ret || !(ret->name = ds_str_dup (name))) {
    free (ret);
    return NULL;
  }
  ret->cpos = 1;
  ret->line = 1;
  return ret;
}

xtree_reader_t *xtree_reader_str (const char *name, const char *input)
{
  xtree_reader_t *ret = xtree_reader_new (name);
  if (!name || !(ret->str_input = ds_str_dup (input))) {
    xtree_reader_free (&ret);
    return ret;
  }
  ret->str_len = strlen (ret->str_input);
  return ret;
}

void xtree_reader_free (xtree_reader_t **reader)
{
  if (!reader || !*reader)
    return;

  xtree_reader_t *r = *reader;
  free (r->name);
  free (r->str_input);

  xtree_errobj_reset (&r->err, 0);
  free (r);
  *reader = NULL;
}

const struct xtree_errobj_t *xtree_reader_error (const xtree_reader_t *reader)
{
  return reader &&
    (reader->err.libcode ||
     reader->err.syscode) ? &reader->err : NULL;
}

bool xtree_reader_eof (const xtree_reader_t *reader)
{
  if (reader->str_index >= reader->str_len) {
    return true;
  } else {
    return false;
  }
}

size_t xtree_reader_line (const xtree_reader_t *reader)
{
  return reader ? reader->line : 0;
}

size_t xtree_reader_cpos (const xtree_reader_t *reader)
{
  return reader ? reader->cpos : 0;
}

static int xtree_reader_getnext (xtree_reader_t *reader)
{
  if (reader->str_index >= reader->str_len ||
      reader->str_input[reader->str_index] == 0) {
    return EOF;
  }
  return reader->str_input[reader->str_index++];
}

int xtree_reader_get (xtree_reader_t *reader)
{
  int c = xtree_reader_getnext (reader);
  if (c == EOF)
    return c;

  if (c == '\n') {
    reader->line++;
    reader->last_cpos = reader->cpos;
    reader->cpos = 1;
  } else {
    reader->cpos++;
  }
  return c;
}

void xtree_reader_unget (xtree_reader_t *reader)
{
  if (reader->str_index) {
    reader->str_index--;
    if (reader->cpos > 1) {
      reader->cpos--;
    }
    if (reader->str_input[reader->str_index] == '\n') {
      reader->cpos = reader->last_cpos;
      reader->line--;
    }
  }
}

int xtree_reader_peek (xtree_reader_t *reader)
{
  int ret = reader->str_input[reader->str_index];
  if (ret == 0)
    ret = EOF;
  return ret;
}



