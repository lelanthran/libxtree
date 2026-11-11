#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "ds_str.h"

#include "xtree_err.h"
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
  bool error = true;
  xtree_scan_token_t *ret = calloc (1, sizeof *ret);
  if (!ret ||
      !(ret->text = malloc (len + 1)) ||
      !(ret->filename = ds_str_dup (filename)))
    goto cleanup;

  ret->id = id;
  ret->line = line;
  ret->cpos = cpos;
  memcpy (ret->text, text, len);
  ret->text[len] = 0;

  error = false;
cleanup:
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

void xtree_scan_token_free (xtree_scan_token_t **token)
{
  if (!token || !*token)
    return;

  free ((*token)->text);
  free ((*token)->filename);
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

unsigned char *xtree_scan_token_dump (const xtree_scan_token_t *token,
                                      unsigned char **dst)
{
  if (!dst || !token)
    return NULL;
  free (*dst); *dst = NULL;
  ds_str_printf ((char **)dst,
                 "token-id:        %i\n"
                 "token-text:      [%s]\n"
                 "token-filename:  [%s]\n"
                 "token-line:      [%zu]\n"
                 "token-cpos:      [%zu]",
                 token->id,
                 token->text,
                 token->filename,
                 token->line,
                 token->cpos);
  return *dst;
}




void xtree_scan_token_errv (struct xtree_errobj_t *err,
                            xtree_scan_token_t *token,
                            enum xtree_errcode_t errcode,
                            const char *fmts,
                            va_list ap)
{
  char *tmp1 = NULL,
       *tmp2 = NULL,
       *msg = NULL;

  ds_str_printf (&tmp1, "[%s:%i] [%s] : ",
                 xtree_scan_token_filename (token),
                 xtree_scan_token_line (token),
                 xtree_scan_token_text (token));
  ds_str_vprintf (&tmp2, fmts, ap);

  if (!tmp1 || !tmp2 || !(msg = ds_str_cat (tmp1, tmp2, NULL))) {
    ERROR (err, xtree_errcode_OOM, "OOM");
  } else {
    ERROR (err, errcode, "%s", msg);
  }
  free (tmp1);
  free (tmp2);
  free (msg);
}

void xtree_scan_token_err (struct xtree_errobj_t *err,
                           xtree_scan_token_t *token,
                           enum xtree_errcode_t errcode,
                           const char *fmts,
                           ...)
{
  va_list ap;
  va_start (ap, fmts);
  xtree_scan_token_errv (err, token, errcode, fmts, ap);
  va_end (ap);
}

bool xtree_scan_token_id_assertv (struct xtree_errobj_t *err,
                                  xtree_scan_token_t *token,
                                  int id, va_list ap)
{
  int tokenid = xtree_scan_token_id (token);
  while (id != XTREE_SCAN_TOKEN_SENTINEL) {
    if (id == tokenid)
      return true;
    id = va_arg (ap, int);
  }
  xtree_scan_token_err (err, token, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected token");
  return false;
}

bool xtree_scan_token_id_assert (struct xtree_errobj_t *err,
                                 xtree_scan_token_t *token,
                                 int id, ...)
{
  va_list ap;
  va_start (ap, id);
  bool ret = xtree_scan_token_id_assertv (err, token, id, ap);
  va_end (ap);
  return ret;
}




