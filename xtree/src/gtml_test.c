#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


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

static int test_gtml_scan (void)
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
   (const unsigned char *)"    And a bogus ampersand:                     &nope;",
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

  static const struct {
    int id;
    const char *text;
  } expected[] = {
    /* <!doctype gtml> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_EXCL,    "!" },
    { xtree_gtml_token_SYMBOL,  "doctype" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "gtml" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <Root Node="Document" empty1 empty2= empty3=""> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "Root" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "Node" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"Document\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "empty1" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "empty2" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "empty3" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"\"" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <# This comment lives at document level. ... #> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_HASH,    "#" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "This" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "comment" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "lives" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "at" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "document" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "level." },
    { xtree_gtml_token_WSPACE,  "\n   " },
    { xtree_gtml_token_SYMBOL,  "The" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "body" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "may" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "contain" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "tags" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_SYMBOL,  "," },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&entities;" },
    { xtree_gtml_token_SYMBOL,  "," },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "and" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_RBRACKET,"]" },
    { xtree_gtml_token_RBRACKET,"]" },
    { xtree_gtml_token_SYMBOL,  "." },
    { xtree_gtml_token_WSPACE,  "\n   " },
    { xtree_gtml_token_SYMBOL,  "Comments" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "do" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "not" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "nest" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "and" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "end" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "only" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "at" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "the" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "terminator." },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_HASH,    "#" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <![CDATA[ ... ]]> at document level (body only, no delimiters) */
    { xtree_gtml_token_CDATA,
      "  CDATA at document level.\n"
      "  Tags like <this> are NOT scanned here, nor are &amp; entities.\n"
      "  Whitespace   is   preserved   verbatim.\n"
      "  Case Is Preserved, Too.\n" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <?directive target="doc-level" ?> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_SYMBOL,  "directive" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "target" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"doc-level\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <SelfClosing id="one" id="two" bare extra="" noQuote=NakedName ... /> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "SelfClosing" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "id" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"one\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "id" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"two\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "bare" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "extra" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "noQuote" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "NakedName" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "num" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "42" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "neg" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "-7" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "dec" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "3.14" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "hex" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "0xFF" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <parent> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "parent" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* <child a=x b=1 c=007 d=-42 e=3.14 f=0xFF g='Two Words' h="..." i=> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "child" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "a" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "x" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "b" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "1" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "c" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "007" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "d" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "-42" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "e" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "3.14" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "f" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "0xFF" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "g" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "'Two Words'" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "h" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_STRING,  "\"Mixed Case Kept\"" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "i" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n    " },

    /* Text with entities: &lt; &gt; &amp; &#65; &#x41; &#0065; */
    { xtree_gtml_token_SYMBOL,  "Text" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "with" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "entities:" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&lt;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&gt;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&amp;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&#65;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&#x41;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&#0065;" },
    { xtree_gtml_token_WSPACE,  "\n    " },

    /* And a bogus ampersand:                     &nope; */
    { xtree_gtml_token_SYMBOL,  "And" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "a" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "bogus" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "ampersand:" },
    { xtree_gtml_token_WSPACE,  "                     " },  /* 21 spaces */
    { xtree_gtml_token_ECODE,   "&nope;" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* </child> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "child" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* <child/> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "child" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* </parent> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "parent" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <MixedCaseTag ATTR=LowerCaseUnquoted COUNT=99> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "MixedCaseTag" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "ATTR" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "LowerCaseUnquoted" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "COUNT" },
    { xtree_gtml_token_EQUAL,   "=" },
    { xtree_gtml_token_SYMBOL,  "99" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* <inner>Some Text</inner> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "inner" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_SYMBOL,  "Some" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "Text" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "inner" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* <?inline-directive not scanned for <tags> or &entities; ?> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_SYMBOL,  "inline-directive" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "not" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "scanned" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "for" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "tags" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "or" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_ECODE,   "&entities;" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n  " },

    /* <![CDATA[ literal content ]]> (body only, no delimiters) */
    { xtree_gtml_token_CDATA,   " literal content " },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* </MixedCaseTag> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "MixedCaseTag" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <UPPER></UPPER> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "UPPER" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "UPPER" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <lower></LOWER> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "lower" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "LOWER" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <top2>Second top-level element makes this a forest.</top2> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "top2" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_SYMBOL,  "Second" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "top-level" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "element" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "makes" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "this" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "a" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "forest." },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "top2" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* <directiveOnly><?a?><?b c?></directiveOnly> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SYMBOL,  "directiveOnly" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_SYMBOL,  "a" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_SYMBOL,  "b" },
    { xtree_gtml_token_WSPACE,  " " },
    { xtree_gtml_token_SYMBOL,  "c" },
    { xtree_gtml_token_QMARK,   "?" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "directiveOnly" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },

    /* </Root> */
    { xtree_gtml_token_LT,      "<" },
    { xtree_gtml_token_SLASH,   "/" },
    { xtree_gtml_token_SYMBOL,  "Root" },
    { xtree_gtml_token_GT,      ">" },
    { xtree_gtml_token_WSPACE,  "\n" },
  };
  static const size_t nexpected = sizeof expected / sizeof expected[0];

  xtree_reader_t *reader = NULL;
  unsigned char *input = NULL;
  struct xtree_errobj_t err = {};
  xtree_scan_token_t *token = NULL;

  if (!(input = makestr (input_array))) {
    PERROR ("OOM creating input\n");
    goto cleanup;
  }

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to init the error object\n");
    goto cleanup;
  }

  if (!(reader = xtree_reader_str ("inline source", input))) {
    PERROR ("Failed to create a reader for the string\n");
    goto cleanup;
  }

  size_t i = 0;
  while ((token = xtree_gtml_scan (&err, reader)) &&
         (xtree_scan_token_id (token)) > 0) {
    if (i >= nexpected) {
      PERROR ("More tokens than expected (%zu/%zu)\n", i, nexpected);
      goto cleanup;
    }
    int got_id = xtree_scan_token_id (token);
    const unsigned char *got_text = xtree_scan_token_text (token);

    const char *msg = NULL;
    if ((strcmp ((char *)got_text, expected[i].text)) != 0) {
      msg = "Wrong text";
    }
    if (got_id != expected[i].id) {
      msg = "Wrong ID";
    }
    if (msg) {
      PERROR ("token #%zu error\nExpected `%s`: `%s`\nGot `%s`: `%s`\n",
              i,
              xtree_gtml_token_id_str (expected[i].id),
              expected[i].text,
              xtree_gtml_token_id_str (got_id),
              got_text);
      ERROR ((&err), xtree_errcode_UNEXPECTED_TOKEN, "Unexpected token returned");
      goto cleanup;
    }
    xtree_scan_token_free (&token);
    i++;
  }

  if (i != nexpected) {
    ERROR ((&err), xtree_errcode_OUT_OF_BOUNDS,
           "Checked %zu/%zu tokens, not all tokens returned\n",
           i, nexpected);
  }
  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  ret = dumperr (&err);
  xtree_errobj_reset (&err, 0);
  xtree_scan_token_free (&token);
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
    TEST (test_gtml_scan),
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
