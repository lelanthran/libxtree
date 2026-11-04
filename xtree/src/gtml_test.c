#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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

#if 0
static void dumperr (struct xtree_errobj_t *err)
{
  if (err->libcode || err->syscode) {
    xtree_errobj_dump (err, stdout);
  }
}

static int ustrcmp (const unsigned char *a, const unsigned char *b)
{
  return strcmp ((const char *)a, (const char *)b);
}

#endif


/* ------------------------------------------------------------------
 * Tests for xtree_gtml_scan()
 * ------------------------------------------------------------------ */


static unsigned char *makestr (const unsigned char **sa)
{
  bool error = true;
  unsigned char *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new ((unsigned char *)"");

  for (size_t i=0; sa[i]; i++) {
    if (!(ds_strbuilder_append (sb, sa[i], NULL)))
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

static int test_gtml_basic (void)
{
  int ret = EXIT_FAILURE;

  static const unsigned char *input_array[] = {
   (const unsigned char *)"<!doctype gtml>",
   (const unsigned char *)"<Root Node=\"Document\" empty1 empty2= empty3=\"\">",
   (const unsigned char *)"<# This comment lives at document level.",
   (const unsigned char *)"   The body may contain <tags>, &entities;, and ]].",
   (const unsigned char *)"   Comments do not nest and end only at the terminator. #>",
   (const unsigned char *)"<![CDATA[  CDATA at document level.",
   (const unsigned char *)"  Tags like <this> are NOT scanned here, nor are &amp; entities.",
   (const unsigned char *)"  Whitespace   is   preserved   verbatim.",
   (const unsigned char *)"  Case Is Preserved, Too.",
   (const unsigned char *)"]]>",
   (const unsigned char *)"<?directive target=\"doc-level\" ?>",
   (const unsigned char *)"<SelfClosing id=\"one\" id=\"two\" bare extra=\"\" noQuote=NakedName num=42 neg=-7 dec=3.14 hex=0xFF/>",
   (const unsigned char *)"<parent>",
   (const unsigned char *)"  <child a=x b=1 c=007 d=-42 e=3.14 f=0xFF g='Two Words' h=\"Mixed Case Kept\" i=>",
   (const unsigned char *)"    Text with entities: &lt; &gt; &amp; &#65; &#x41; &#0065;",
   (const unsigned char *)"    And a bogus ampersand: &nope; and a lone one: & here.",
   (const unsigned char *)"  </child>",
   (const unsigned char *)"  <child/>",
   (const unsigned char *)"</parent>",
   (const unsigned char *)"<MixedCaseTag ATTR=LowerCaseUnquoted COUNT=99>",
   (const unsigned char *)"  <inner>Some Text</inner>",
   (const unsigned char *)"  <?inline-directive not scanned for <tags> or &entities; ?>",
   (const unsigned char *)"  <![CDATA[ literal content ]]>",
   (const unsigned char *)"</MixedCaseTag>",
   (const unsigned char *)"<UPPER></UPPER>",
   (const unsigned char *)"<lower></LOWER>",
   (const unsigned char *)"<top2>Second top-level element makes this a forest.</top2>",
   (const unsigned char *)"<directiveOnly><?a?><?b c?></directiveOnly>",
   (const unsigned char *)"</Root>",
  NULL
  };
  unsigned char *input = makestr (input_array);

  if (!input) {
    PERROR ("OOM creating input\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  free (input);
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
    TEST (test_gtml_basic),
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
      printf ("[%50s]: ", tests[i].name);
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
