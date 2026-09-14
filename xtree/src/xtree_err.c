#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


#include "xtree_err.h"

const char *xtree_errcode_str (enum xtree_errcode_t code)
{
#define ERRCODE(x)    { x, #x }
  static const struct {
    enum xtree_errcode_t code;
    const char *s;
  } errcodes[] = {
    ERRCODE (xtree_errcode_NONE),
    ERRCODE (xtree_errcode_OOM),
    ERRCODE (xtree_errcode_PARAMETER_TYPE),
    ERRCODE (xtree_errcode_ATTRIBUTE_NOT_FOUND),
    ERRCODE (xtree_errcode_OUT_OF_BOUNDS),
    ERRCODE (xtree_errcode_INTERNAL_STATE),
    ERRCODE (xtree_errcode_CHILD_NOT_FOUND),
    ERRCODE (xtree_errcode_NULL_PARAMETER),
  };
  static const size_t nerrcodes = sizeof errcodes / sizeof errcodes[0];
#undef ERRCODE

  for (size_t i=0; i < nerrcodes; i++) {
    if (errcodes[i].code == code)
      return errcodes[i].s;
  }

  static char unknown[64];
  snprintf (unknown, sizeof unknown, "%i: unknown code", code);
  return unknown;
}

bool xtree_errobj_reset (struct xtree_errobj_t *err, size_t msglen)
{
  free (err->message);
  memset (err, 0, sizeof *err);
  if (!msglen)
    return true;

  err->message_len = msglen;
  if (!(err->message = calloc (1, err->message_len))) {
    err->message_len = 0;
    return false;
  }
  return true;
}

void xtree_errobj_clrerr (struct xtree_errobj_t *err)
{
  if (!err)
    return;

  size_t len = err->message_len;
  char *tmp = err->message;
  memset (err, 0, sizeof *err);
  err->message_len = len;
  err->message = tmp;
}

void xtree_errobj_dump (const struct xtree_errobj_t *err, FILE *outf)
{
  if (!outf)
    outf = stdout;
  if (!err) {
    fprintf (outf, "NULL error object\n");
    return;
  }

  fprintf (outf,
           "srcfile:        [%s]\n"
           "srcline:        [%i]\n"
           "syserr:         [%i]\n"
           "sysmsg:         [%s]\n"
           "liberr:         [%i]\n"
           "libmsg:         [%s]\n"
           "msglen:         [%zu]\n"
           "message:        [%s]\n",
           err->srcfile, err->srcline,
           err->syscode, err->sysmsg,
           err->libcode, err->libmsg,
           err->message_len, err->message);
}

