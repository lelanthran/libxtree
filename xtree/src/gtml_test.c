#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>


#include <unistd.h>


#include "ds_str.h"
#include "ds_strbuilder.h"


#include "xtree_err.h"
#include "xtree.h"
#include "xtree_reader.h"
#include "xtree_scan.h"

#include "xtree_gtml_scan.h"
#include "xtree_gtml_parse.h"


/* All xtree text (names, values, attribute keys, input) is unsigned char *
 * so that UTF-8 survives. U() gives a string literal (or char buffer) that
 * type; the u* helpers are the unsigned char * equivalents of the string.h
 * functions the tests need. */
#define U(s)    ((const unsigned char *)(s))
#define CS(s)   ((const char *)(s))   /* for printf-style %s only */


#define PERROR(...)   do { \
  fprintf (stderr, "%s:%i ", __FILE__, __LINE__); \
  fprintf (stderr, __VA_ARGS__); \
} while (0)


static int dumperr (struct xtree_errobj_t *err)
{
  if (err->libcode || err->syscode) {
    xtree_errobj_dump (err, stdout);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

#if 0
static int ustrcmp (const unsigned char *a, const unsigned char *b)
{
  return strcmp ((const char *)a, (const char *)b);
}
#endif


/* ------------------------------------------------------------------
 * Tests for xtree_gtml_scan()
 * ------------------------------------------------------------------ */


#if 0
static unsigned char *makestr (const unsigned char **sa)
{
  bool error = true;
  unsigned char *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new ((unsigned char *)"");

  for (size_t i=0; sa[i]; i++) {
    if (!(ds_strbuilder_append (sb, sa[i], "\n", NULL)))
      goto cleanup;
  }

  ret = (unsigned char *)ds_strbuilder_build (sb);
  error = false;

cleanup:
  ds_strbuilder_del (sb);
  if (error) {
    free (ret);
    ret = NULL;
  }
  return ret;
}
#endif

static int test_gtml_scan_happy (void)
{
  int ret = EXIT_FAILURE;


  static const struct {
    const char *name;
    xtree_scan_token_t *(*fptr) (struct xtree_errobj_t *, xtree_reader_t *);
    enum xtree_gtml_token_id_t id;
    const char *input;
    const char *output;
    const char *remaining;
  } tests[] = {
    {
      "whitespace",   xtree_gtml_scan_wspace,     xtree_gtml_token_WSPACE,
      " \n \r\n\t \f <....",
      " \n \r\n\t \f ",
      "<...",
    },
    {
      "equal1",        xtree_gtml_scan_equal,      xtree_gtml_token_EQUAL,
      "=ddd",
      "=",
      "ddd",
    },
    {
      "cdata",        xtree_gtml_scan_cdata,      xtree_gtml_token_CDATA,
      "<![CDATA[ <something goes here> ]]>s",
      " <something goes here> ",
      "s",
    },
    {
      "tagname1",      xtree_gtml_scan_tagname,    xtree_gtml_token_TAGNAME,
      "<test-tagname name1=value1 />",
      "test-tagname",
      " name1=value1",
    },
    {
      "tagname2",      xtree_gtml_scan_tagname,    xtree_gtml_token_TAGNAME,
      "<test-tagname>",
      "test-tagname",
      ">",
    },
    {
      "tagname3",   xtree_gtml_scan_tagname, xtree_gtml_token_TAGNAME,
      "<br/>",
      "br",
      "/>",
    },
    {
      "tagname4",   xtree_gtml_scan_tagname, xtree_gtml_token_TAGNAME,
      "<a\nx>",
      "a",
      "\nx>",
    },
    {
      "close-tag1",     xtree_gtml_scan_close_tag,  xtree_gtml_token_CLOSE_TAG,
      "</tagname> ",
      "tagname",
      ">",
    },
    {
      "close-tag2",     xtree_gtml_scan_close_tag,  xtree_gtml_token_CLOSE_TAG,
      "</tagname   >s",
      "tagname",
      ">",
    },
    {
      "attr-name",    xtree_gtml_scan_attr_name,  xtree_gtml_token_ATTR_NAME,
      " name1=value1 />",
      "name1",
      "=",
    },
    {
      "attr-name2",   xtree_gtml_scan_attr_name, xtree_gtml_token_ATTR_NAME,
      " x y>",
      "x",
      " y>",
    },
    {
      "attr-name3",   xtree_gtml_scan_attr_name, xtree_gtml_token_ATTR_NAME,
      " x>",
      "x",
      ">",
    },
    {
      "attr-name4",   xtree_gtml_scan_attr_name, xtree_gtml_token_ATTR_NAME,
      " x/>",
      "x",
      "/>",
    },
    {
      "attr-value1",  xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "value1 />",
      "value1",
      " />",
    },
    {
      "attr-value2",  xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "value2/>",
      "value2",
      "/>",
    },
    {
      "attr-value3",  xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "'value\"3\"' />",
      "value\"3\"",
      " />",
    },
    {
      "attr-value4",  xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "\"value'4'\" />",
      "value'4'",
      " />",
    },
    {
      "attr-value5",   xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "v>",
      "v",
      ">",
    },
    {
      "attr-value6",   xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "\"a b\">",
      "a b",
      ">",
    },
    {
      "attr-value7",   xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "\"\">",
      "",
      ">",
    },
    {
      "attr-value8",   xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      ">",
      "",
      ">",
    },
    {
      "attr-value9",   xtree_gtml_scan_attr_value, xtree_gtml_token_ATTR_VALUE,
      "/>",
      "",
      "/>",
    },
    {
      "self-closing1",  xtree_gtml_scan_sclosing, xtree_gtml_token_SCLOSING,
      "/>s",
      "",
      "s",
    },
    {
      "self-closing2",  xtree_gtml_scan_sclosing, xtree_gtml_token_SCLOSING,
      "   />s",
      "",
      "s",
    },
    {
      "gt1",           xtree_gtml_scan_gt, xtree_gtml_token_GT,
      "> ",
      ">",
      " ",
    },
    {
      "gt2",           xtree_gtml_scan_gt, xtree_gtml_token_GT,
      "  >",
      ">",
      "\xff",
    },
    {
      "content1",      xtree_gtml_scan_content,    xtree_gtml_token_CONTENT,
      " inner content < to be ignored",
      " inner content ",
      "< to be ignored",
    },
    {
      "content2",      xtree_gtml_scan_content,    xtree_gtml_token_CONTENT,
      " inner content &amp; to be ignored",
      " inner content ",
      "&amp; to be ignored",
    },
    {
      "content3",   xtree_gtml_scan_content, xtree_gtml_token_CONTENT,
      "text",
      "text",
      "\xff",
    },
    {
      "content4",   xtree_gtml_scan_content, xtree_gtml_token_CONTENT,
      "a > b\nc<",
      "a > b\nc",
      "<",
    },
    {
      "ecode",        xtree_gtml_scan_ecode,      xtree_gtml_token_ECODE,
      "&foo; s",
      "foo",
      " s",
    },
    {
      "comment1",     xtree_gtml_scan_comment,    xtree_gtml_token_COMMENT,
      "<!-- a comment --> extra content",
      " a comment ",
      " extra content",
    },
    {
      "comment2",     xtree_gtml_scan_comment,    xtree_gtml_token_COMMENT,
      "<!-- a comment --> --> extra content",
      " a comment ",
      " --> extra content",
    },
    {
      "directive",    xtree_gtml_scan_directive,  xtree_gtml_token_DIRECTIVE,
      "<? <some> <dir> goes <here> ?> s",
      " <some> <dir> goes <here> ",
      " s",
    },
  };
  static const size_t ntests = sizeof tests / sizeof tests[0];

  xtree_reader_t *reader = NULL;
  struct xtree_errobj_t err = {};
  xtree_scan_token_t *token = NULL;

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to init the error object\n");
    err.libcode = xtree_errcode_OOM;
    goto cleanup;
  }

  for (size_t i=0; i < ntests; i++) {
    // Setup a reader for input
    xtree_reader_free (&reader);
    if (!(reader = xtree_reader_str ("inline source", U(tests[i].input)))) {
      PERROR ("Failed to create a reader for the string\n");
      ERROR ((&err), xtree_errcode_OOM,
             "[%s]: Failed to init reader", tests[i].name);
      goto cleanup;
    }

    // Scan the next expected token, then compare it
    xtree_scan_token_free (&token);
    if (!(token = tests[i].fptr (&err, reader))) {
      PERROR ("[%s]: Failed to scan\n", tests[i].name);
      goto cleanup;
    }
    int got_id = xtree_scan_token_id (token);
    const unsigned char *got_text = xtree_scan_token_text (token);

    const char *msg = NULL;
    if ((strcmp ((char *)got_text, tests[i].output)) != 0) {
      msg = "Wrong text";
    }
    if (got_id != tests[i].id) {
      msg = "Wrong ID";
    }
    if (msg) {
      PERROR ("test [%s]: error\nExpected `%s`: `%s`\nGot `%s`: `%s`\n",
              tests[i].name,
              xtree_gtml_token_id_str (tests[i].id),
              tests[i].output,
              xtree_gtml_token_id_str (got_id),
              got_text);
      goto cleanup;
    }

    // Ensure we have the correct remaining characters in the reader
    if ((xtree_reader_peek (reader)) != tests[i].remaining[0]) {
      PERROR ("test [%s]: expected `%c` 0x%02x remaining, got `%c`  0x%02x instead\n",
              tests[i].name,
              tests[i].remaining[0], tests[i].remaining[0],
              xtree_reader_peek (reader), xtree_reader_peek (reader));
      goto cleanup;
    }
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  if ((dumperr (&err)) != 0)
    ret = EXIT_FAILURE;
  xtree_errobj_reset (&err, 0);
  xtree_scan_token_free (&token);
  return ret;
}

int main (int argc, char **argv)
{
  int errcount = 0;
  size_t testcount = 0;
#define TEST(x)     { #x, false, x }

  static struct {
    const char *name;
    bool selected;
    int (*fptr) (void);
  } tests[] = {

    /* Tests for xtree_gtml_scan() */
    TEST (test_gtml_scan_happy),
  };
#undef TEST

  static const size_t ntests = sizeof tests / sizeof tests[0];

  // Determine which tests should be selected for execution.
  for (size_t i=0; i<ntests; i++) {
    if (argc == 1) {
      tests[i].selected = true;
      continue;
    }
    if (argc > 1 && (strcmp (argv[1], "all") == 0)) {
      tests[i].selected = true;
      continue;
    }
    for (int j=1; j<argc; j++) {
      if ((strcmp (argv[j], tests[i].name) == 0)) {
        tests[i].selected = true;
      } else {
      }
    }
  }

  printf ("testing xtree_gtml library\n");
  for (size_t i=0; i<ntests; i++) {
    if (tests[i].selected) {
      printf ("[%30s]: ", tests[i].name);
      fflush (stdout);
      int rc = tests[i].fptr ();
      printf ("%10s\n", rc ? "failed" : "passed");
      errcount += rc ? 1 : 0;
      testcount++;
    }
  }
  if (errcount) {
    printf ("FAILED: %i/%zu tests failed\n", errcount, testcount);
  } else {
    printf ("PASSED: %zu tests passed\n", testcount);
  }
  return errcount;
}
