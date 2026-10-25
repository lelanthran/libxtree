#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ds_str.h"

#include "xtree_reader.h"
#include "xtree_scan.h"

struct xtree_scan_token_t {
  int id;
  unsigned char *text;
  char *filename;
  size_t line;
  size_t cpos;
};


xtree_scan_token_t *xtree_scan_token_new (int id,
                                          const char *filename,
                                          size_t line, size_t cpos,
                                          const unsigned char *text, size_t len)
{
  xtree_scan_token_t *ret = calloc (1, sizeof *ret);
  if (!ret ||
      !(ret->text = malloc (len + 1)) ||
      !(ret->filename = ds_str_dup (filename)))
    return NULL;

  ret->id = id;
  ret->line = line;
  ret->cpos = cpos;
  memcpy (ret->text, text, len);
  ret->text[len] = 0;

  return ret;
}

void xtree_scan_token_free (xtree_scan_token_t **token)
{
  if (!token || !*token)
    return;

  free ((*token)->text);
  free (*token);
  *token = NULL;
}

int xtree_scan_token_id (const xtree_scan_token_t *token)
{
  return token ? token->id : -1;
}

void xtree_scan_token_id_set (xtree_scan_token_t *token, int id)
{
  if (token) {
    token->id = id;
  }
}

const unsigned char *xtree_scan_token_text (const xtree_scan_token_t *token)
{
  return token ? token->text : NULL;
}

const char *xtree_scan_token_filename (const xtree_scan_token_t *token)
{
  return token ? token->filename : NULL;
}

size_t xtree_scan_token_line (const xtree_scan_token_t *token)
{
  return token ? token->line : 0;
}

size_t xtree_scan_token_cpos (const xtree_scan_token_t *token)
{
  return token ? token->cpos : 0;
}

