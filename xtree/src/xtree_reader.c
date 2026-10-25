#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ds_str.h"
#include "xtree_err.h"
#include "xtree_reader.h"

/* Read an entire file into a single string. Caller must free the returned
 * buffer. Returns NULL on error. */
static unsigned char *fslurp (FILE *inf)
{
  bool error = true;
  unsigned char *ret = NULL;
  size_t retlen = 0;
#define BUFSIZE   (1024 * 1024)
// #define BUFSIZE   (2)
  unsigned char *buf = NULL;
  size_t nbytes = 0;
  unsigned char *tmp = NULL;

  if (!(buf = malloc (BUFSIZE)))
    goto cleanup;

  if (!inf)
    goto cleanup;

  while (!(feof (inf)) && !(ferror (inf))) {
    nbytes = fread (buf, 1, BUFSIZE, inf);
    if (!(tmp = realloc (ret, retlen + nbytes + 1)))
      goto cleanup;
    ret = tmp;
    memcpy (&ret[retlen], buf, nbytes);
    retlen += nbytes;
    ret[retlen] = 0;
  }
  if (ferror (inf))
    goto cleanup;

  error = false;
cleanup:
  free (buf);
  if (error) {
    free (ret);
    ret = NULL;
  }
  return ret;
}




struct xtree_reader_t {
  // Inputs provided by caller
  char *name;
  unsigned char *input;
  size_t input_len;
  size_t input_idx;

  // Track state of reader
  struct xtree_errobj_t err;

};

static xtree_reader_t *xtree_reader_new (const char *name, unsigned char *input)
{
  xtree_reader_t *ret = calloc (1, sizeof *ret);
  if (!input || !ret || !(ret->name = ds_str_dup (name))) {
    free (ret);
    return NULL;
  }
  ret->input = input;
  ret->input_len = strlen ((char *)input);
  return ret;
}

xtree_reader_t *xtree_reader_str (const char *name, const unsigned char *input)
{
  return xtree_reader_new (name, (unsigned char *)ds_str_dup ((char *)input));
}

xtree_reader_t *xtree_reader_file (const char *name, FILE *input)
{
  return xtree_reader_new (name, fslurp (input));
}

void xtree_reader_free (xtree_reader_t **reader)
{
  if (!reader || !*reader)
    return;

  xtree_reader_t *r = *reader;
  free (r->name);
  free (r->input);

  xtree_errobj_reset (&r->err, 0);
  free (r);
  *reader = NULL;
}

const char *xtree_reader_name (const xtree_reader_t *reader)
{
  return reader ? reader->name : NULL;
}

const struct xtree_errobj_t *xtree_reader_error (const xtree_reader_t *reader)
{
  return reader &&
    (reader->err.libcode ||
     reader->err.syscode) ? &reader->err : NULL;
}

bool xtree_reader_eof (const xtree_reader_t *reader)
{
  if (!reader)
    return true;
  if (reader->input_idx >= reader->input_len ||
      reader->input[reader->input_idx] == 0) {
    return true;
  }
  return false;
}

size_t xtree_reader_line (const xtree_reader_t *reader)
{
  if (!reader)
    return 0;
  size_t line = 1;
  for (size_t i = 0; i < reader->input_idx; i++) {
    if (reader->input[i] == '\n') {
      line++;
    }
  }
  return line;
}

size_t xtree_reader_cpos (const xtree_reader_t *reader)
{
  if (!reader)
    return 0;
  size_t cpos = 1;
  for (size_t i = 0; i < reader->input_idx; i++) {
    if (reader->input[i] == '\n') {
      cpos = 1;
    } else {
      cpos++;
    }
  }
  return cpos;
}

int xtree_reader_get (xtree_reader_t *reader)
{
  if (!reader || reader->input_idx >= reader->input_len ||
      reader->input[reader->input_idx] == 0) {
    return EOF;
  }
  return reader->input[reader->input_idx++];
}

void xtree_reader_unget (xtree_reader_t *reader)
{
  if (reader && reader->input_idx > 0) {
    reader->input_idx--;
  }
}

int xtree_reader_peek (xtree_reader_t *reader)
{
  if (!reader || reader->input_idx >= reader->input_len)
    return EOF;
  int ret = reader->input[reader->input_idx];
  if (ret == 0)
    ret = EOF;
  return ret;
}
