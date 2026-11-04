#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <unistd.h>


/* Small test program to test the json functions in the xtree library */
#include "ds_str.h"
#include "xtree_err.h"
#include "xtree.h"
#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_json_scan.h"
#include "xtree_json_parse.h"


/* All xtree text (names, values, attribute keys, input) is unsigned char *
 * so that UTF-8 survives. U() gives a string literal (or char buffer) that
 * type; the u* helpers are the unsigned char * equivalents of the string.h
 * functions the tests need. */
#define U(s)    ((const unsigned char *)(s))
#define CS(s)   ((const char *)(s))   /* for printf-style %s only */

static int ustrcmp (const unsigned char *a, const unsigned char *b)
{
  return strcmp ((const char *)a, (const char *)b);
}

#define PERROR(...)   do { \
  fprintf (stderr, "%s:%i ", __FILE__, __LINE__); \
  fprintf (stderr, __VA_ARGS__); \
} while (0)

static void dumperr (struct xtree_errobj_t *err)
{
  if (err->libcode || err->syscode) {
    xtree_errobj_dump (err, stdout);
  }
}


/* Create a temporary file under /tmp. On success *dst_fname is set to a
 * heap-allocated filename (caller must free) and *dst_outf to an open
 * FILE* (caller must fclose). Returns true on success, false on
 * error. */
static bool fdump (char **dst_fname, FILE **dst_outf)
{
  if (!dst_fname || !dst_outf)
    return false;

  *dst_fname = NULL;
  *dst_outf = NULL;

  char tmpl[] = "/tmp/xtree_dump_XXXXXX";
  int fd = mkstemp (tmpl);
  if (fd < 0)
    return false;

  FILE *outf = fdopen (fd, "w+");
  if (!outf) {
    close (fd);
    remove (tmpl);
    return false;
  }

  char *fname = ds_str_dup (tmpl);
  if (!fname) {
    fclose (outf);
    remove (tmpl);
    return false;
  }

  *dst_fname = fname;
  *dst_outf = outf;
  return true;
}

/* Report every field of an error object returned by the reader. */
static void rdr_print_errobj (const struct xtree_errobj_t *err)
{
  PERROR ("errobj: libcode=%i libmsg=[%.*s] syscode=%i sysmsg=[%.*s] "
          "src=[%.*s:%i] message=[%s] message_len=%zu\n",
          (int)err->libcode, (int)sizeof err->libmsg, err->libmsg,
          err->syscode, (int)sizeof err->sysmsg, err->sysmsg,
          (int)sizeof err->srcfile, err->srcfile, err->srcline,
          err->message ? err->message : "", err->message_len);
}

/* Check that the reader has no error: xtree_reader_error() must return
 * NULL. If it does not, the contents of the error object are reported.
 * Returns true if there is no error. */
static bool rdr_expect_noerror (const xtree_reader_t *reader)
{
  const struct xtree_errobj_t *err = xtree_reader_error (reader);
  if (err) {
    PERROR ("Reader should not have an error object\n");
    rdr_print_errobj (err);
    return false;
  }
  return true;
}

/* Check that an error object returned by the reader is well-formed: it
 * records an error (libcode or syscode set), the fixed-size strings are
 * all NUL-terminated within their buffers, and the dynamic message, if
 * present, is NUL-terminated within message_len. Returns true if the
 * object is well-formed. */
static bool rdr_expect_wellformed_errobj (const struct xtree_errobj_t *err)
{
  if (!err) {
    PERROR ("Error object is NULL\n");
    return false;
  }
  if (!err->libcode && !err->syscode) {
    PERROR ("Error object is set but records neither a libcode nor a "
            "syscode\n");
    rdr_print_errobj (err);
    return false;
  }
  if (!memchr (err->sysmsg, '\0', sizeof err->sysmsg) ||
      !memchr (err->libmsg, '\0', sizeof err->libmsg) ||
      !memchr (err->srcfile, '\0', sizeof err->srcfile)) {
    PERROR ("Error object has an unterminated fixed-size string\n");
    return false;
  }
  if (err->message && !memchr (err->message, '\0', err->message_len)) {
    PERROR ("Error object message is not terminated within message_len\n");
    return false;
  }
  return true;
}

/* ------------------------------------------------------------------
 * Tests for xtree_reader_file()
 *
 * Assumptions:
 *   - xtree_reader_file() does NOT take ownership of the FILE*: the test
 *     closes it itself, after freeing the reader.
 *   - The FILE* stays open for the whole life of the reader (the reader
 *     may read lazily).
 *   - Behaviour must be identical to xtree_reader_str() for the same data.
 *
 * Add to the includes:   (nothing new; fdump() already pulls in what we need)
 * Add to the TEST list:  test_reader_file_basic, (const unsigned char *)
 *                        test_reader_file_large,
 *                        test_reader_file_null_input
 * ------------------------------------------------------------------ */

/* Write `len` bytes of `data` to a new temporary file (using fdump() for
 * the name), close it, and return the filename in *dst_fname (caller
 * must remove() and free() it). Returns true on success. */
static bool rdr_write_tmpfile (const char *data, size_t len, char **dst_fname)
{
  FILE *outf = NULL;
  char *fname = NULL;

  if (!fdump (&fname, &outf)) {
    PERROR ("Failed to create temporary file\n");
    return false;
  }

  if (len && fwrite (data, 1, len, outf) != len) {
    PERROR ("Failed to write test data to [%s]\n", fname);
    fclose (outf);
    remove (fname);
    free (fname);
    return false;
  }

  if (fclose (outf) != 0) {
    PERROR ("Failed to close [%s]\n", fname);
    remove (fname);
    free (fname);
    return false;
  }

  *dst_fname = fname;
  return true;
}


/* ------------------------------------------------------------------
 * Tests for xtree_json_scan()
 *
 * This scanner is a lenient, JSON-like tokeniser (see xtree_json_scan.h):
 *
 *   - Every call returns a token. EOF (id 0) is returned at the end of the
 *     input, repeatedly if the scanner is called again. ERROR (id
 *     -1) is returned only for a string that is not terminated
 *     before the end of the input. EOF and ERROR have "" as their text.
 *   - Token text is the raw source text, with two exceptions, both for
 *     STRING tokens. First, the text of a STRING token does NOT include
 *     the surrounding double quotes ("abc" is returned as abc, "" as the
 *     empty string); the token's line/cpos is still that of the opening
 *     quote. Second, inside strings a literal LF is returned as the two
 *     characters \n and a literal CR as \r (so CRLF is returned as \r\n).
 *     Escape sequences in strings are never decoded or validated,
 *     whatever they are; they are returned as written, minus only the
 *     enclosing quotes.
 *   - It tokenises only; it does not check the JSON grammar. Trailing
 *     commas, "}{" and "1 2" all scan without complaint.
 *   - Whitespace is space, tab, CR and LF. It is skipped between tokens.
 *   - "//" starts a comment (anywhere outside a string) that runs to the
 *     end of the line. The comment token's text includes the "//" and
 *     excludes the line terminator (LF, and a CR directly before it).
 *   - Anything else is a "word", classified by its FIRST character and,
 *     for the keywords, by its whole text. The symbol set is
 *
 *         a-z A-Z 0-9  ! @ # $ % ^ & * ( ) _ + = - ; > < ? . ' \
 *
 *     A colon is NOT in the set (it is the COLON token), nor are
 *     { } [ ] , or a double quote, whitespace, a slash (/, which is
 *     reserved for "//" comments, and is UNRECOGNISED when it is not part
 *     of a comment or a NUMBER), or anything else (| ~ `,
 *     control characters, bytes >= 0x80). A SYMBOL is the longest run of
 *     set characters; it ends at the first character outside the set.
 *     A character that is outside the set and cannot start any other token
 *     is UNRECOGNISED. ASSUMPTION: the text of an UNRECOGNISED token is
 *     the whole run of such characters (it ends at whitespace, punctuation,
 *     a quote or a set character).
 *       * The exact words true/false/null/Infinity/NaN/undefined are
 *         their own tokens (see the sign rule below).
 *       * Otherwise, a word that starts with a number character (a digit,
 *         '+', '-' or '.') is a NUMBER. The scanner does NOT validate
 *         numbers, but a NUMBER is NOT delimited like other words: it
 *         extends over the longest run of characters from this set
 *
 *             . - +  0-9  a-f  A-F  x X      (b, B, e, E are in a-f/A-F)
 *
 *         and ends at the first character outside it, whatever that is.
 *         The text of the NUMBER is the raw run, and scanning then
 *         carries on from the character that ended it, as a new token.
 *         So "0xwxyz" is the NUMBER "0x" followed by the SYMBOL "wxyz",
 *         "12px" is the NUMBER "12" then the SYMBOL "px", "1_000" is "1"
 *         then "_000". Slashes do not end a number: "1/2", "1//2" and
 *         "1//c" are each one NUMBER. '/' in any quantity, "//"
 *         included, belongs to the number and never starts a comment
 *         inside one. (A comment after whitespace or another
 *         delimiter, as in "1 //c" or "[1,//c", is still a comment.)
 *         Conversely "1abc",
 *         "123eb", "12..13", "08", "-", "+a", "-x", "0b12" and
 *         "2026-10-25" consist only of set characters, so each is a
 *         single NUMBER with the raw text kept. Rejecting malformed
 *         numbers is the parser's job.
 *       * Everything else is a SYMBOL. A symbol therefore can never start
 *         with a digit, '+', '-' or '.' (nor with any other token
 *         start: { } [ ] : , or a double quote), though it may contain
 *         them after the first character ("x1", "a-b", "a.b.c"). A
 *         symbol is delimited by the symbol set (see above), not by the
 *         number set.
 *     So "truex" is a SYMBOL. A sign in front of
 *     something that is not an exact keyword is just a NUMBER that ends
 *     at the first non-set character.
 *     Infinity and NaN are keywords like the others: only the exact word
 *     is a keyword token, and anything suffixed onto it ("Infinityx",
 *     "Infinity-", "NaNa") is a SYMBOL with the whole word as its text.
 *     There are no signed keyword tokens. A sign in front of a keyword is
 *     a NUMBER token of just the sign, followed by the keyword token:
 *     "+Infinity" and "-Infinity" are NUMBER "+"/"-" then INFINITY
 *     "Infinity", "+NaN" and "-NaN" are NUMBER then NAN "NaN", and
 *     "+undefined", "-null" and so on likewise. A suffixed word is not a
 *     keyword, so "-Infinity2" is NUMBER "-" then SYMBOL "Infinity2".
 *     The parser decides what such pairs mean.
 *
 * Assumptions I had to make (adjust the tables if the library differs):
 *
 *   - Token line/cpos is the position of the token's FIRST character.
 *     As in the reader tests, the absolute origin (0- or 1-based) is not
 *     assumed: positions are expressed relative to the reader's starting
 *     line()/cpos(). The reader resets cpos on '\n', so cpos offsets are
 *     relative to the start of the line even on later lines. '\r' is an
 *     ordinary character that advances cpos. A string that spans lines
 *     keeps the position of its opening quote, and the next token is
 *     positioned after it on the reader's current line.
 *   - The EOF token's position is the reader's position at the end of
 *     the input (after any trailing whitespace has been skipped).
 *   - An ERROR token's line/cpos is the reader's position at the end of
 *     the input (where the missing closing quote was expected).
 *   - A scanner error (the ERROR token) is not a reader error.
 *     xtree_reader_error() returns NULL after every token, ERROR
 *     included; it is only set when the underlying source fails (file
 *     removed, permissions changed, and so on).
 *   - NBSP is not whitespace (it is UNRECOGNISED). Form feed and vertical
 *     tab are not tested: the scanner uses isspace(), which skips them,
 *     contradicting the four-character whitespace rule above.
 *   - Letter case matters: True, NULL, infinity and Undefined are SYMBOLs.
 *     "-undefined" and "+undefined" are not keywords (only the exact
 *     words listed above are), so each is a NUMBER ("+" or "-") followed
 *     by the keyword token that starts at the next character.
 *   - Spelling is exactly "NaN", so "Nan" is a SYMBOL.
 *   - The NEG_INFINITY and NEG_NAN token ids are never produced; the
 *     tests expect a NUMBER "-" and the unsigned keyword token instead.
 *   - A backslash followed directly by a literal LF inside a string is
 *     not tested (the result of rewriting it is ambiguous).
 *   - Nothing is asserted about what the scanner does after an ERROR
 *     token.
 * ------------------------------------------------------------------ */

#define JSN_NAME "test.json"

/* Marks an expected position as "wherever the reader is after the scan
 * call"; used for the EOF token. */
#define JSN_AT_READER ((size_t)-1)

struct jsn_exp {
  enum xtree_json_token_id_t id;
  const char *text;
  size_t line;    /* offset from the reader's starting line() */
  size_t cpos;    /* offset from the reader's starting cpos() */
};

#define JSN_T(id, text, line, cpos)  { (enum xtree_json_token_id_t)(id), (text), (line), (cpos) }
#define JSN_N(a)  (sizeof (a) / sizeof (a)[0])

static const char *jsn_idname (enum xtree_json_token_id_t id)
{
  switch (id) {
    case xtree_json_token_RESERVED:       return "RESERVED";
    case xtree_json_token_ERROR:          return "ERROR";
    case xtree_json_token_EOF:            return "EOF";
    case xtree_json_token_LBRACE:         return "LBRACE";
    case xtree_json_token_RBRACE:         return "RBRACE";
    case xtree_json_token_LBRACKET:       return "LBRACKET";
    case xtree_json_token_RBRACKET:       return "RBRACKET";
    case xtree_json_token_COLON:          return "COLON";
    case xtree_json_token_COMMA:          return "COMMA";
    case xtree_json_token_STRING:         return "STRING";
    case xtree_json_token_NUMBER:         return "NUMBER";
    case xtree_json_token_TRUE:           return "TRUE";
    case xtree_json_token_FALSE:          return "FALSE";
    case xtree_json_token_NULL:           return "NULL";
    case xtree_json_token_COMMENT:        return "COMMENT";
    case xtree_json_token_SYMBOL:         return "SYMBOL";
    case xtree_json_token_INFINITY:       return "INFINITY";
    case xtree_json_token_NEG_INFINITY:   return "NEG_INFINITY";
    case xtree_json_token_NAN:            return "NAN";
    case xtree_json_token_NEG_NAN:        return "NEG_NAN";
    case xtree_json_token_UNDEFINED:      return "UNDEFINED";
    case xtree_json_token_UNRECOGNISED:   return "UNRECOGNISED";
  }
  /* No default case above: the compiler (-Wswitch) then reports any token
   * id added to the enum that this function does not name. */
  return "(unknown id)";
}

/* The input and token index being checked, so that every failure message
 * can say what input was scanned. Set by jsn_run(). */
static const char *jsn_cur_input = "(none)";
static size_t jsn_cur_idx = 0;
static char jsn_in_buf[512];

/* Copy `in` into jsn_in_buf, showing control and non-ASCII bytes as
 * \n, \r, \t or \xNN so that padded inputs are readable. */
static void jsn_set_input (const char *in)
{
  size_t o = 0;
  for (; *in && o + 5 < sizeof (jsn_in_buf); in++) {
    unsigned char c = (unsigned char)*in;
    if (c == '\n')      { jsn_in_buf[o++] = '\\'; jsn_in_buf[o++] = 'n'; }
    else if (c == '\r') { jsn_in_buf[o++] = '\\'; jsn_in_buf[o++] = 'r'; }
    else if (c == '\t') { jsn_in_buf[o++] = '\\'; jsn_in_buf[o++] = 't'; }
    else if (c < 0x20 || c >= 0x7f)
      o += (size_t)snprintf (jsn_in_buf + o, 5, "\\x%02x", c);
    else
      jsn_in_buf[o++] = (char)c;
  }
  jsn_in_buf[o] = '\0';
  jsn_cur_input = jsn_in_buf;
}

static const char *dumphex (const unsigned char *buf, size_t index) {
  static char ret[3][1024 * 1024];
  size_t i = 0;
  snprintf (ret[index], sizeof ret, "[%s] ", (char *)buf);
  size_t j = strlen (ret[index]);
  while (buf[i]) {
    snprintf (&ret[index][j], 6, "0x%02x-", buf[i++]);
    j += 5;
  }
  return ret[index];
}


/* Scan one token from the reader and check its id, text, filename,
 * position and the reader's error state. `line` and `cpos` are absolute
 * (JSN_AT_READER for both means "the reader's position after the call").
 * The token is freed. Returns true if everything matched. */
static bool jsn_check_token (xtree_reader_t *reader, enum xtree_json_token_id_t id,
                             const unsigned char *text, size_t line, size_t cpos)
{
  bool ok = false;
  xtree_scan_token_t *tok = xtree_json_scan (reader);

  if (!tok) {
    PERROR ("For input [%.80s] token %zu: expected [%s] (id %d) with text "
            "[%.80s], got NULL\n", dumphex((const unsigned char *)jsn_cur_input, 0), jsn_cur_idx,
            jsn_idname (id), (int)id, text);
    return false;
  }

  if (line == JSN_AT_READER) {
    line = xtree_reader_line (reader);
    cpos = xtree_reader_cpos (reader);
  }

  enum xtree_json_token_id_t gid = (enum xtree_json_token_id_t)xtree_scan_token_id (tok);
  const unsigned char *gtext = xtree_scan_token_text (tok);
  const char *gname = xtree_scan_token_filename (tok);
  size_t gline = xtree_scan_token_line (tok);
  size_t gcpos = xtree_scan_token_cpos (tok);
  const char *gt = gtext ? (const char *)gtext : "(null)";

  if (gid != id) {
    PERROR ("For input [%.80s] token %zu: expected id [%s] (%d) with text "
            "[%.80s], got id [%s] (%d) with text [%.80s]\n",
            dumphex((const unsigned char *)jsn_cur_input, 0), jsn_cur_idx, jsn_idname (id), (int)id,
            dumphex (text, 1),
            jsn_idname (gid), (int)gid, dumphex ((const unsigned char *)gt, 2));
    goto cleanup;
  }
  if (!gtext || ustrcmp (gtext, text) != 0) {
    PERROR ("For input [%.80s] token %zu: expected id [%s] (%d) with text "
            "[%.80s], got id [%s] (%d) with text [%.80s]\n",
            dumphex((const unsigned char *)jsn_cur_input, 0), jsn_cur_idx, jsn_idname (id), (int)id,
            dumphex (text, 1),
            jsn_idname (gid), (int)gid, dumphex ((const unsigned char *)gt, 2));
    goto cleanup;
  }
  if (!gname || strcmp (gname, JSN_NAME) != 0) {
    PERROR ("For input [%.80s] token %zu [%s] text [%.80s]: expected "
            "filename [%s], got [%s]\n", dumphex((const unsigned char *)jsn_cur_input, 0), jsn_cur_idx,
            jsn_idname (id), text, JSN_NAME, gname ? CS (gname) : "(null)");
    goto cleanup;
  }
  if (gline != line || gcpos != cpos) {
    PERROR ("For input [%.80s] token %zu [%s] text [%.80s]: expected "
            "line %zu cpos %zu, got line %zu cpos %zu\n",
            dumphex((const unsigned char *)jsn_cur_input, 0), jsn_cur_idx, jsn_idname (id),
            dumphex (text, 1),
            line, cpos, gline, gcpos);
    goto cleanup;
  }

  /* A scanner error (an ERROR token, e.g. an unterminated string) is not
   * a reader error: the reader reports an error only when the underlying
   * source is broken. So the reader must report no error after every
   * token, ERROR included. */
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ok = true;

cleanup:
  xtree_scan_token_free (&tok);
  return ok;
}

/* Check that the scanner is at the end of the input: it must return EOF
 * (twice, because EOF is sticky) and the reader must report EOF. */
static bool jsn_check_eof (xtree_reader_t *reader)
{
  for (size_t i=0; i<2; i++) {
    if (!jsn_check_token (reader, xtree_json_token_EOF, U (""),
                          JSN_AT_READER, JSN_AT_READER))
      return false;
  }
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should be at EOF after the EOF token\n");
    return false;
  }
  return true;
}

/* Scan `input` and check that it produces exactly the n tokens in `want`
 * followed by EOF. If the last expected token is an ERROR, nothing after
 * it is checked. Positions in `want` are relative to the reader's
 * starting position. */
static bool jsn_run (const char *input, const struct jsn_exp *want, size_t n)
{
  bool ok = false;
  xtree_reader_t *reader = xtree_reader_str (JSN_NAME, (const unsigned char *)input);

  if (!reader) {
    PERROR ("Failed to create reader\n");
    return false;
  }

  size_t line0 = xtree_reader_line (reader);
  size_t cpos0 = xtree_reader_cpos (reader);

  jsn_set_input (input);
  for (size_t i=0; i<n; i++) {
    jsn_cur_idx = i;
    if (!jsn_check_token (reader, want[i].id, (const unsigned char *)want[i].text,
                          line0 + want[i].line, cpos0 + want[i].cpos)) {
      goto cleanup;
    }
  }
  jsn_cur_idx = n;

  if (!(n && want[n-1].id == xtree_json_token_ERROR)) {
    if (!jsn_check_eof (reader)) {
      PERROR ("Bad end of input [%.80s]\n", input);
      goto cleanup;
    }
  }

  ok = true;

cleanup:
  xtree_reader_free (&reader);
  return ok;
}

/* Input that must produce a NUMBER token followed by one other token. The
 * number ends at the first character outside the number set, and the
 * remainder is scanned afresh. The second token starts right after the
 * number on the same line. Each input is tried bare and padded with
 * whitespace on both sides (the padding puts the number on line 1). */
struct jsn_split_case {
  const char *input;
  const char *num;       /* expected NUMBER text */
  enum xtree_json_token_id_t restid;  /* expected id of the following token */
  const char *rest;      /* expected text of the following token */
};

static int jsn_run_split_cases (const struct jsn_split_case *cases, size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    size_t nlen = strlen (cases[i].num);
    struct jsn_exp e[2] = {
      JSN_T (xtree_json_token_NUMBER, cases[i].num, 0, 0),
      JSN_T (cases[i].restid, cases[i].rest, 0, nlen),
    };
    if (!jsn_run (cases[i].input, e, 2))
      rc = EXIT_FAILURE;

    size_t sz = strlen (cases[i].input) + 16;
    char *padded = malloc (sz);
    if (!padded) {
      PERROR ("Failed to allocate padded input\n");
      rc = EXIT_FAILURE;
      continue;
    }
    snprintf (padded, sz, " \t\r\n%s\n\r\t ", cases[i].input);
    e[0].line = 1;
    e[1].line = 1;
    if (!jsn_run (padded, e, 2))
      rc = EXIT_FAILURE;
    free (padded);
  }
  return rc;
}

/* Input that must produce a single ERROR token at (line, cpos). */
struct jsn_bad_case {
  const char *input;
  size_t line;
  size_t cpos;
};

static int jsn_run_bad_cases (const struct jsn_bad_case *cases, size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    struct jsn_exp e = JSN_T (xtree_json_token_ERROR, "",
                              cases[i].line, cases[i].cpos);
    if (!jsn_run (cases[i].input, &e, 1))
      rc = EXIT_FAILURE;
  }
  return rc;
}

/* Each input must be scanned as exactly one token of type `id` whose text
 * is the input itself. Each is tried bare and padded with whitespace on
 * both sides (the padding puts the token on the second line).
 *
 * The exception is id == STRING: each input must then be a complete,
 * double-quoted string, and the expected token text is the input without
 * its first and last character (the quotes). */
static int jsn_run_single_cases (enum xtree_json_token_id_t id, const char *const *inputs,
                                 size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    size_t ilen = strlen (inputs[i]);
    char *want = NULL;

    if (id == xtree_json_token_STRING) {
      if (ilen < 2 || inputs[i][0] != '"' || inputs[i][ilen - 1] != '"') {
        PERROR ("Test bug: STRING input [%.80s] is not double-quoted\n",
                inputs[i]);
        rc = EXIT_FAILURE;
        continue;
      }
      want = malloc (ilen - 1);
      if (!want) {
        PERROR ("Failed to allocate expected text\n");
        rc = EXIT_FAILURE;
        continue;
      }
      memcpy (want, inputs[i] + 1, ilen - 2);
      want[ilen - 2] = '\0';
    }

    struct jsn_exp e = JSN_T (id, want ? want : inputs[i], 0, 0);
    if (!jsn_run (inputs[i], &e, 1))
      rc = EXIT_FAILURE;

    size_t sz = ilen + 16;
    char *padded = malloc (sz);
    if (!padded) {
      PERROR ("Failed to allocate padded input\n");
      rc = EXIT_FAILURE;
      free (want);
      continue;
    }
    snprintf (padded, sz, " \t\r\n%s\n\r\t ", inputs[i]);
    e.line = 1;
    e.cpos = 0;
    if (!jsn_run (padded, &e, 1))
      rc = EXIT_FAILURE;
    free (padded);
    free (want);
  }
  return rc;
}

// name: test_json_scan_eof_empty
static int test_json_scan_eof_empty (void)
{
  // No tokens: EOF straight away, at the starting position.
  return jsn_run ("", NULL, 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_eof_whitespace_only
static int test_json_scan_eof_whitespace_only (void)
{
  static const char *const inputs[] = {
    " ", "\t", "\r", "\n", " \t\r\n", "\n\n\n", "  \n\t", "\r\n\r\n",
  };
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<JSN_N (inputs); i++) {
    if (!jsn_run (inputs[i], NULL, 0))
      rc = EXIT_FAILURE;
  }
  return rc;
}

// name: test_json_scan_punctuation_each
static int test_json_scan_punctuation_each (void)
{
  int rc = EXIT_SUCCESS;
  static const struct { enum xtree_json_token_id_t id; const char *text; } p[] = {
    { xtree_json_token_LBRACE,   "{" },
    { xtree_json_token_RBRACE,   "}" },
    { xtree_json_token_LBRACKET, "[" },
    { xtree_json_token_RBRACKET, "]" },
    { xtree_json_token_COLON,    ":" },
    { xtree_json_token_COMMA,    "," },
  };
  for (size_t i=0; i<JSN_N (p); i++) {
    if (jsn_run_single_cases (p[i].id, &p[i].text, 1) != EXIT_SUCCESS)
      rc = EXIT_FAILURE;
  }
  return rc;
}

// name: test_json_scan_punctuation_sequence
static int test_json_scan_punctuation_sequence (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 0),
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 1),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 2),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 3),
    JSN_T (xtree_json_token_COLON,    ":", 0, 4),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 5),
  };
  return jsn_run ("{}[]:,", want, JSN_N (want)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_punctuation_whitespace_separated
static int test_json_scan_punctuation_whitespace_separated (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 1),
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 3),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 5),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 7),
    JSN_T (xtree_json_token_COLON,    ":", 0, 9),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 11),
  };
  return jsn_run (" { } [ ] : , ", want, JSN_N (want))
    ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_grammar_agnostic
static int test_json_scan_grammar_agnostic (void)
{
  // The scanner only tokenises; it must not reject grammatically invalid
  // token streams.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp backwards[] = {
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 0),
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 1),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 2),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 3),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 4),
    JSN_T (xtree_json_token_COLON,    ":", 0, 5),
  };
  if (!jsn_run ("}{][,:", backwards, JSN_N (backwards)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp mixed[] = {
    JSN_T (xtree_json_token_NUMBER, "1",   0, 0),
    JSN_T (xtree_json_token_STRING, "a", 0, 2),
    JSN_T (xtree_json_token_TRUE,   "true", 0, 6),
    JSN_T (xtree_json_token_NULL,   "null", 0, 11),
  };
  if (!jsn_run ("1 \"a\" true null", mixed, JSN_N (mixed)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp double_comma[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 3),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 4),
  };
  if (!jsn_run ("[1,,]", double_comma, JSN_N (double_comma)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_literals_each
static int test_json_scan_literals_each (void)
{
  static const char *const t[] = { "true" };
  static const char *const f[] = { "false" };
  static const char *const n[] = { "null" };
  int rc = EXIT_SUCCESS;

  if (jsn_run_single_cases (xtree_json_token_TRUE, t, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_FALSE, f, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_NULL, n, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  return rc;
}

// name: test_json_scan_literals_sequence
static int test_json_scan_literals_sequence (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_TRUE,  "true",  0, 0),
    JSN_T (xtree_json_token_FALSE, "false", 0, 5),
    JSN_T (xtree_json_token_NULL,  "null",  0, 11),
  };
  return jsn_run ("true false null", want, JSN_N (want))
    ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_keyword_vs_lookalike
static int test_json_scan_keyword_vs_lookalike (void)
{
  // The exact keyword gets the keyword id; a word that merely starts with
  // it is a SYMBOL carrying its full text.
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_TRUE,      "true",      0, 0),
    JSN_T (xtree_json_token_SYMBOL,    "truex",     0, 5),
    JSN_T (xtree_json_token_FALSE,     "false",     0, 11),
    JSN_T (xtree_json_token_SYMBOL,    "falsey",    0, 17),
    JSN_T (xtree_json_token_NULL,      "null",      0, 24),
    JSN_T (xtree_json_token_SYMBOL,    "nullable",  0, 29),
    JSN_T (xtree_json_token_UNDEFINED, "undefined", 0, 38),
    JSN_T (xtree_json_token_SYMBOL,    "undefinedx", 0, 48),
  };
  return jsn_run ("true truex false falsey null nullable undefined undefinedx",
                  want, JSN_N (want)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_literals_in_structure
static int test_json_scan_literals_in_structure (void)
{
  // Literals directly followed by punctuation, with no whitespace.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp arr[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",     0, 0),
    JSN_T (xtree_json_token_TRUE,     "true",  0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",     0, 5),
    JSN_T (xtree_json_token_FALSE,    "false", 0, 6),
    JSN_T (xtree_json_token_COMMA,    ",",     0, 11),
    JSN_T (xtree_json_token_NULL,     "null",  0, 12),
    JSN_T (xtree_json_token_RBRACKET, "]",     0, 16),
  };
  if (!jsn_run ("[true,false,null]", arr, JSN_N (arr)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp obj[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "k", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",     0, 4),
    JSN_T (xtree_json_token_TRUE,   "true",  0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",     0, 9),
  };
  if (!jsn_run ("{\"k\":true}", obj, JSN_N (obj)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_strings_valid
static int test_json_scan_strings_valid (void)
{
  // Each string is returned without its enclosing double quotes, with
  // every escape left exactly as written. The C literals below are the raw
  // JSON text, quotes included; the expected token text is each literal
  // minus its first and last character.
  static const char *const cases[] = {
    "\"\"",                                   /* empty */
    "\" \"",                                  /* just a space */
    "\"abc\"",
    "\"hello world\"",
    "\"\\\"\"",                               /* "\""   */
    "\"\\\\\"",                               /* "\\"   */
    "\"a\\\\\"",                              /* "a\\"  escaped backslash
                                                 right before the quote */
    "\"\\\\n\"",                              /* "\\n"  backslash, then n */
    "\"\\/\"",                                /* "\/"   */
    "\"\\b\\f\\n\\r\\t\"",                    /* all the short escapes */
    "\"\\u0041\"",
    "\"\\u00e9\\u00E9\\uabcd\\uABCD\\u0000\"", /* hex case, \u0000 */
    "\"\\uD83D\\uDE00\"",                     /* surrogate pair, raw */
    "\"/\"",                                  /* unescaped solidus */
    "\"'\"",                                  /* single quote */
    "\"{}[]:,\"",                             /* structural characters */
    "\"true false null 123\"",
    "\"// not a comment\"",
    "\"a//b\"",
    "\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"", /* UTF-8, raw bytes */
    "\"\x7f\"",                               /* DEL */
    "\"a\tb\"",                               /* literal tab stays literal */
    "\"\x01\"",                               /* so do other controls */
  };
  return jsn_run_single_cases (xtree_json_token_STRING, cases, JSN_N (cases));
}

// name: test_json_scan_strings_sequence
static int test_json_scan_strings_sequence (void)
{
  int rc = EXIT_SUCCESS;

  // A key/value pair, and two strings with nothing between them (the
  // scanner does not require separators).
  static const struct jsn_exp pair[] = {
    JSN_T (xtree_json_token_STRING, "a", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":",     0, 3),
    JSN_T (xtree_json_token_STRING, "b", 0, 4),
  };
  if (!jsn_run ("\"a\":\"b\"", pair, JSN_N (pair)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp adjacent[] = {
    JSN_T (xtree_json_token_STRING, "a", 0, 0),
    JSN_T (xtree_json_token_STRING, "b", 0, 3),
  };
  if (!jsn_run ("\"a\"\"b\"", adjacent, JSN_N (adjacent)))
    rc = EXIT_FAILURE;

  // "a\\" ends at the second quote, because the backslash before it is
  // itself escaped; the comma and number after it are separate tokens.
  static const struct jsn_exp escaped_backslash[] = {
    JSN_T (xtree_json_token_STRING, "a\\\\", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",         0, 5),
    JSN_T (xtree_json_token_NUMBER, "1",         0, 6),
  };
  if (!jsn_run ("\"a\\\\\",1", escaped_backslash, JSN_N (escaped_backslash)))
    rc = EXIT_FAILURE;

  // An escaped quote does not end the string.
  static const struct jsn_exp escaped_quote[] = {
    JSN_T (xtree_json_token_STRING, "a\\\",b", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",           0, 7),
  };
  if (!jsn_run ("\"a\\\",b\",", escaped_quote, JSN_N (escaped_quote)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_numbers_valid
static int test_json_scan_numbers_valid (void)
{
  // Returned as raw text; nothing is normalised ("1.0" stays "1.0",
  // "1E10" stays "1E10", "-0" stays "-0", "0x1F" stays "0x1F").
  static const char *const cases[] = {
    /* decimal integers */
    "0", "-0", "+0", "1", "-1", "+1", "9", "10", "123", "1234567890",
    "-1234567890", "+1234567890",
    "12345678901234567890123456789012345678901234567890",
    /* fractions, including a missing integer or fraction part */
    "0.5", "-0.5", "+0.5", ".5", "-.5", "+.5", "0.", "5.", "-5.",
    "0.0", "-0.0", "1.0", "10.25", "0.1", "123.456",
    /* exponents */
    "1e10", "1E10", "1e+10", "1e-10", "1E+10", "1E-2", "1e0", "0e0",
    "-0e-0", "0.0e+0", "0e1", "+1e1", "-1.5e+3", ".5e3", "1.e5",
    "123.456E789", "1.7976931348623157e308", "5e-324", "1e999999",
    /* hexadecimal */
    "0x0", "0x1", "0xF", "0xff", "0XFF", "0xdeadBEEF", "0x1abc",
    "-0x10", "+0x10", "0x0123456789abcdefABCDEF",
    /* octal (a leading zero followed by octal digits) */
    "00", "01", "07", "0777", "-017", "+07",
    /* binary */
    "0b0", "0b1", "0b1010", "0B11", "-0b1", "+0b1", "0b00001111",
  };
  return jsn_run_single_cases (xtree_json_token_NUMBER, cases, JSN_N (cases));
}

// name: test_json_scan_numbers_malformed
static int test_json_scan_numbers_malformed (void)
{
  // The scanner does not validate numbers. A word that starts with a
  // number character (a digit, '+', '-' or '.') is a NUMBER that extends
  // over the characters in the number set  .-+ 0-9 a-f A-F x X  and stops
  // at the first character outside it. Deciding that these are not valid
  // numbers is left to the parser.

  // Inputs made only of number-set characters are a single NUMBER with
  // the raw text kept.
  static const char *const cases[] = {
    /* a digit followed by junk that is all in the set */
    "1/2", "1/2/3", "1/", "1//", "1//2", "1///c", "1//c", "-x//c", "1/2//3",
    ".//", "1abc", "1e", "1E", "1e+", "1e-", "1e+x", "1.5e", "1.5.5",
    "1.2.3", "1e5e5", "0.5.", "5..", "12..13", "123eb",
    "2026-10-25", "1-2", "1+1", "1fFaA", "1xX",
    /* bad octal, hex and binary */
    "08", "09", "007e", "00.5", "01.5", "012e3",
    "0x", "0X", "0x-1", "-0x", "0b", "0B",
    "0b2", "0b12", "+0b", "0x1.8", "0b1.1", "0xx", "0bb", "0xabcdefABCDEF",
    /* signs and dots with no digits, or followed by set letters */
    "-", "+", "-a", "+a", "--1", "-+1", "+-1", ".", "..", "..5", ".e5",
    "-.", "+.", ".x", "-x", "+x", ".-", ".+", "-f", "+F", ".b", "-X",
  };
  int rc = jsn_run_single_cases (xtree_json_token_NUMBER, cases,
                                 JSN_N (cases));

  // Inputs with a character outside the set: the NUMBER ends there and
  // the rest is scanned as the next token (a SYMBOL here, a keyword
  // when the remainder is exactly a keyword, or UNRECOGNISED when the
  // next character is outside the symbol set).
  static const struct jsn_split_case split[] = {
    /* a digit followed by a character outside the set */
    { "12px",   "12",  xtree_json_token_SYMBOL, "px" },
    { "1_000",  "1",   xtree_json_token_SYMBOL, "_000" },
    { "1(2)",   "1",   xtree_json_token_SYMBOL, "(2)" },
    { "1'2",    "1",   xtree_json_token_SYMBOL, "'2" },
    { "1@",     "1",   xtree_json_token_SYMBOL, "@" },
    { "1\\",    "1",   xtree_json_token_SYMBOL, "\\" },
    { "1g",     "1",   xtree_json_token_SYMBOL, "g" },
    { "1G",     "1",   xtree_json_token_SYMBOL, "G" },
    { "1z",     "1",   xtree_json_token_SYMBOL, "z" },
    /* bad octal, hex and binary */
    { "0o17",   "0",   xtree_json_token_SYMBOL, "o17" },
    { "0xg",    "0x",  xtree_json_token_SYMBOL, "g" },
    { "0xwxyz", "0x",  xtree_json_token_SYMBOL, "wxyz" },
    { "0x1g",   "0x1", xtree_json_token_SYMBOL, "g" },
    /* a sign in front of something that is not an exact keyword */
    { "+Infinit",   "+",  xtree_json_token_SYMBOL,   "Infinit" },
    { "--Infinity", "--", xtree_json_token_INFINITY, "Infinity" },
    { "++Infinity", "++", xtree_json_token_INFINITY, "Infinity" },
    { "+-Infinity", "+-", xtree_json_token_INFINITY, "Infinity" },
    { "-infinity",  "-",  xtree_json_token_SYMBOL,   "infinity" },
    { "+infinity",  "+",  xtree_json_token_SYMBOL,   "infinity" },
    { "-undefined", "-",  xtree_json_token_UNDEFINED, "undefined" },
    { "+undefined", "+",  xtree_json_token_UNDEFINED, "undefined" },
    { "-true",      "-",  xtree_json_token_TRUE,     "true" },
    { "+true",      "+",  xtree_json_token_TRUE,     "true" },
    { "-null",      "-",  xtree_json_token_NULL,     "null" },
    { "+null",      "+",  xtree_json_token_NULL,     "null" },
    /* a sign in front of an exact keyword is its own NUMBER token, then
     * the keyword token (there are no signed keyword tokens) */
    { "+Infinity",  "+",  xtree_json_token_INFINITY, "Infinity" },
    { "-Infinity",  "-",  xtree_json_token_INFINITY, "Infinity" },
    { "+NaN",       "+",  xtree_json_token_NAN,      "NaN" },
    { "-NaN",       "-",  xtree_json_token_NAN,      "NaN" },
    /* ... and a keyword with anything suffixed onto it is not a keyword,
     * so it is a SYMBOL after the sign's NUMBER */
    { "+Infinity2", "+",  xtree_json_token_SYMBOL,   "Infinity2" },
    { "-Infinity2", "-",  xtree_json_token_SYMBOL,   "Infinity2" },
    { "-Infinityx", "-",  xtree_json_token_SYMBOL,   "Infinityx" },
    { "+Infinity-", "+",  xtree_json_token_SYMBOL,   "Infinity-" },
    { "+NaN2",      "+",  xtree_json_token_SYMBOL,   "NaN2" },
    { "-NaN2",      "-",  xtree_json_token_SYMBOL,   "NaN2" },
    { "-NaNa",      "-",  xtree_json_token_SYMBOL,   "NaNa" },
    { "+NaN-",      "+",  xtree_json_token_SYMBOL,   "NaN-" },
    /* "f" and "a" are in the set, "l" is not */
    { "+false",     "+fa", xtree_json_token_SYMBOL,  "lse" },
    /* characters outside the symbol set after the number are
     * UNRECOGNISED (form feed and vertical tab are not tested: whether
     * they are whitespace is not yet specified) */
    { "1\x01",     "1",  xtree_json_token_UNRECOGNISED, "\x01" },
    { "-\xff",     "-",  xtree_json_token_UNRECOGNISED, "\xff" },
    { ".\xc3\xa9", ".",  xtree_json_token_UNRECOGNISED, "\xc3\xa9" },
    { "1\xc2\xa0", "1",  xtree_json_token_UNRECOGNISED, "\xc2\xa0" },
    { "1|",        "1",  xtree_json_token_UNRECOGNISED, "|" },
    { "0x1~",      "0x1", xtree_json_token_UNRECOGNISED, "~" },
  };
  if (jsn_run_split_cases (split, JSN_N (split)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;

  // Malformed numbers are still delimited by structural characters.
  static const struct jsn_exp delimited[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",      0, 0),
    JSN_T (xtree_json_token_NUMBER,   "12..13", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",      0, 7),
    JSN_T (xtree_json_token_NUMBER,   "0x",     0, 8),
    JSN_T (xtree_json_token_SYMBOL,   "wxyz",   0, 10),
    JSN_T (xtree_json_token_COMMA,    ",",      0, 14),
    JSN_T (xtree_json_token_NUMBER,   "-",      0, 15),
    JSN_T (xtree_json_token_RBRACKET, "]",      0, 16),
  };
  if (!jsn_run ("[12..13,0xwxyz,-]", delimited, JSN_N (delimited)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp object[] = {
    JSN_T (xtree_json_token_LBRACE, "{",      0, 0),
    JSN_T (xtree_json_token_STRING, "a",  0, 1),
    JSN_T (xtree_json_token_COLON,  ":",      0, 4),
    JSN_T (xtree_json_token_NUMBER, "123eb",  0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",      0, 10),
  };
  if (!jsn_run ("{\"a\":123eb}", object, JSN_N (object)))
    rc = EXIT_FAILURE;

  // A number ending at a string, a comment, or whitespace.
  static const struct jsn_exp then_string[] = {
    JSN_T (xtree_json_token_NUMBER, "1x",    0, 0),
    JSN_T (xtree_json_token_STRING, "s", 0, 2),
  };
  if (!jsn_run ("1x\"s\"", then_string, JSN_N (then_string)))
    rc = EXIT_FAILURE;

  // Slashes belong to the number: no comment starts inside one.
  static const struct jsn_exp then_comment[] = {
    JSN_T (xtree_json_token_NUMBER, "-x//c", 0, 0),
  };
  if (!jsn_run ("-x//c", then_comment, JSN_N (then_comment)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_NUMBER, "1abc", 0, 0),
    JSN_T (xtree_json_token_NUMBER, ".",    0, 5),
    JSN_T (xtree_json_token_SYMBOL, "abc",  0, 7),
  };
  if (!jsn_run ("1abc . abc", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  // Any number of slashes, "//" included, stays inside the number; the
  // comment only starts after the number has ended.
  static const struct jsn_exp slashes[] = {
    JSN_T (xtree_json_token_NUMBER, "1//2", 0, 0),
  };
  if (!jsn_run ("1//2", slashes, JSN_N (slashes)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp slash_then_comment[] = {
    JSN_T (xtree_json_token_NUMBER,  "1/2",  0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c",  0, 4),
  };
  if (!jsn_run ("1/2 //c", slash_then_comment, JSN_N (slash_then_comment)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_numbers_delimited
static int test_json_scan_numbers_delimited (void)
{
  // A number ends at the first character outside the number set, which
  // includes whitespace, { } [ ] : , and a double quote. Slashes, "//"
  // included, do not end a number.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp comma[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",", 0, 1),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
  };
  if (!jsn_run ("1,2", comma, JSN_N (comma)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp list[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",   0, 0),
    JSN_T (xtree_json_token_NUMBER,   "0",   0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",   0, 2),
    JSN_T (xtree_json_token_NUMBER,   "-0",  0, 3),
    JSN_T (xtree_json_token_COMMA,    ",",   0, 5),
    JSN_T (xtree_json_token_NUMBER,   "0.5", 0, 6),
    JSN_T (xtree_json_token_RBRACKET, "]",   0, 9),
  };
  if (!jsn_run ("[0,-0,0.5]", list, JSN_N (list)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp object[] = {
    JSN_T (xtree_json_token_LBRACE, "{",        0, 0),
    JSN_T (xtree_json_token_STRING, "a",    0, 1),
    JSN_T (xtree_json_token_COLON,  ":",        0, 4),
    JSN_T (xtree_json_token_NUMBER, "-1.5e+3",  0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",        0, 12),
  };
  if (!jsn_run ("{\"a\":-1.5e+3}", object, JSN_N (object)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
    JSN_T (xtree_json_token_NUMBER, "3", 0, 4),
  };
  if (!jsn_run ("1 2 3", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp newline[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "2", 1, 0),
  };
  if (!jsn_run ("1\n2", newline, JSN_N (newline)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp colon[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":", 0, 1),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
  };
  if (!jsn_run ("1:2", colon, JSN_N (colon)))
    rc = EXIT_FAILURE;

  // "0x1 abc" is a number and a symbol (the space ends the word), while
  // "0x1abc" (see test_json_scan_numbers_valid) is a single number.
  static const struct jsn_exp hex_then_symbol[] = {
    JSN_T (xtree_json_token_NUMBER, "0x1", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "abc", 0, 4),
  };
  if (!jsn_run ("0x1 abc", hex_then_symbol, JSN_N (hex_then_symbol)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_whitespace_kinds
static int test_json_scan_whitespace_kinds (void)
{
  int rc = EXIT_SUCCESS;

  // Tabs.
  static const struct jsn_exp tabs[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 2),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 3),
    JSN_T (xtree_json_token_NUMBER,   "2", 0, 5),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 7),
  };
  if (!jsn_run ("[\t1,\t2\t]", tabs, JSN_N (tabs)))
    rc = EXIT_FAILURE;

  // CRLF line endings: only '\n' starts a new line; '\r' is skipped like
  // any other whitespace.
  static const struct jsn_exp crlf[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "a", 1, 2),
    JSN_T (xtree_json_token_COLON,  ":",     1, 6),
    JSN_T (xtree_json_token_NUMBER, "1",     2, 1),
    JSN_T (xtree_json_token_RBRACE, "}",     3, 0),
  };
  if (!jsn_run ("{\r\n  \"a\" :\r\n 1\r\n}\r\n", crlf, JSN_N (crlf)))
    rc = EXIT_FAILURE;

  // A mix of everything, with blank lines and leading/trailing space.
  static const struct jsn_exp mixed[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 1, 1),
    JSN_T (xtree_json_token_NUMBER,   "1", 3, 2),
  };
  if (!jsn_run (" \t\r\n [ \n\n  1 ", mixed, JSN_N (mixed)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_full_document
static int test_json_scan_full_document (void)
{
  static const char *const input =
    "{\n"
    "  \"name\": \"x\",\n"
    "  \"list\": [1, 2.5, -3e2, true, false, null],\n"
    "  \"obj\": {\"k\": \"v\\n\"}\n"
    "}\n";

  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{",        0, 0),
    JSN_T (xtree_json_token_STRING,   "name", 1, 2),
    JSN_T (xtree_json_token_COLON,    ":",        1, 8),
    JSN_T (xtree_json_token_STRING,   "x",    1, 10),
    JSN_T (xtree_json_token_COMMA,    ",",        1, 13),
    JSN_T (xtree_json_token_STRING,   "list", 2, 2),
    JSN_T (xtree_json_token_COLON,    ":",        2, 8),
    JSN_T (xtree_json_token_LBRACKET, "[",        2, 10),
    JSN_T (xtree_json_token_NUMBER,   "1",        2, 11),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 12),
    JSN_T (xtree_json_token_NUMBER,   "2.5",      2, 14),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 17),
    JSN_T (xtree_json_token_NUMBER,   "-3e2",     2, 19),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 23),
    JSN_T (xtree_json_token_TRUE,     "true",     2, 25),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 29),
    JSN_T (xtree_json_token_FALSE,    "false",    2, 31),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 36),
    JSN_T (xtree_json_token_NULL,     "null",     2, 38),
    JSN_T (xtree_json_token_RBRACKET, "]",        2, 42),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 43),
    JSN_T (xtree_json_token_STRING,   "obj",  3, 2),
    JSN_T (xtree_json_token_COLON,    ":",        3, 7),
    JSN_T (xtree_json_token_LBRACE,   "{",        3, 9),
    JSN_T (xtree_json_token_STRING,   "k",    3, 10),
    JSN_T (xtree_json_token_COLON,    ":",        3, 13),
    JSN_T (xtree_json_token_STRING,   "v\\n", 3, 15),
    JSN_T (xtree_json_token_RBRACE,   "}",        3, 20),
    JSN_T (xtree_json_token_RBRACE,   "}",        4, 0),
  };
  return jsn_run (input, want, JSN_N (want)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_token_filename
static int test_json_scan_token_filename (void)
{
  // The filename on each token is the name the reader was created with.
  // (jsn_check_token() checks JSN_NAME; this checks a different name, so a
  // hard-coded filename in the scanner would be caught.)
  int ret = EXIT_FAILURE;
  static const char *const name = "dir/other-name.json";
  xtree_reader_t *reader = xtree_reader_str (name, U ("[1]"));
  xtree_scan_token_t *tok = NULL;

  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  for (size_t i=0; i<4; i++) {     /* '[', '1', ']', EOF */
    tok = xtree_json_scan (reader);
    if (!tok) {
      PERROR ("Scanner returned NULL for token %zu\n", i);
      goto cleanup;
    }
    const char *got = xtree_scan_token_filename (tok);
    if (!got || strcmp (got, name) != 0) {
      PERROR ("Token %zu: expected filename [%s], got [%s]\n",
              i, name, got ? CS (got) : "(null)");
      goto cleanup;
    }
    xtree_scan_token_free (&tok);
    tok = NULL;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&tok);
  xtree_reader_free (&reader);
  return ret;
}

// name: test_json_scan_string_escapes_not_decoded
static int test_json_scan_string_escapes_not_decoded (void)
{
  // The scanner neither decodes nor validates escapes: whatever follows a
  // backslash is kept as written, including sequences that are not valid
  // JSON. Each text equals its input minus the enclosing quotes.
  static const char *const cases[] = {
    "\"\\q\"",
    "\"\\x41\"",
    "\"\\u\"",                    /* \u with no digits */
    "\"\\u12\"",                  /* too few digits */
    "\"\\u12G4\"",                /* non-hex digit */
    "\"\\uXYZW\"",
    "\"\\'\"",
    "\"\\a\"",
    "\"\\0\"",
    "\"\\U0041\"",
    "\"\\e\"",
    "\"\\ \"",                    /* backslash space */
    "\"\\\\q\"",                  /* escaped backslash, then q */
    "\"\\n\"",                    /* the two characters \ and n */
    "\"\\r\\n\"",
    "\"C:\\\\dir\\file\"",
    "\"\\u0041\\\\u0041\"",
  };
  return jsn_run_single_cases (xtree_json_token_STRING, cases, JSN_N (cases));
}

// name: test_json_scan_strings_multiline
static int test_json_scan_strings_multiline (void)
{
  // A string may contain literal newlines. In the returned text (which
  // excludes the enclosing quotes) a literal LF becomes \n and a literal
  // CR becomes \r, so CRLF becomes \r\n.
  // Tabs, other characters and escapes already present are untouched.
  int rc = EXIT_SUCCESS;

  static const struct { const char *input; const char *text; } cases[] = {
    { "\"a\nb\"",       "a\\nb" },
    { "\"\n\"",         "\\n" },
    { "\"\n\n\n\"",     "\\n\\n\\n" },
    { "\"a\r\nb\"",     "a\\r\\nb" },
    { "\"a\rb\"",       "a\\rb" },
    { "\"\r\n\r\n\"",   "\\r\\n\\r\\n" },
    { "\"a\tb\"",       "a\tb" },
    { "\"a\n\tb\"",     "a\\n\tb" },
    { "\"a\\nb\"",      "a\\nb" },         /* already escaped: as-is */
    { "\"x\\n\ny\"",    "x\\n\\ny" },      /* escaped, then literal */
    { "\"a\\\"\nb\"",   "a\\\"\\nb" },     /* escaped quote, then LF */
    { "\"a\n// not\"",  "a\\n// not" },    /* not a comment */
    { "\"{\n}\"",       "{\\n}" },
    { "\"line one\nline two\nline three\"",
      "line one\\nline two\\nline three" },
  };
  for (size_t i=0; i<JSN_N (cases); i++) {
    struct jsn_exp e = JSN_T (xtree_json_token_STRING, cases[i].text, 0, 0);
    if (!jsn_run (cases[i].input, &e, 1))
      rc = EXIT_FAILURE;
  }

  // Positions: the string keeps the position of its opening quote, and
  // the tokens after it are positioned on the line the reader is now on.
  static const struct jsn_exp then_number[] = {
    JSN_T (xtree_json_token_STRING, "a\\nb", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "1",         1, 3),
  };
  if (!jsn_run ("\"a\nb\" 1", then_number, JSN_N (then_number)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",             0, 0),
    JSN_T (xtree_json_token_STRING,   "a\\r\\nb",  0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",             1, 2),
    JSN_T (xtree_json_token_NUMBER,   "1",             2, 2),
    JSN_T (xtree_json_token_RBRACKET, "]",             2, 3),
  };
  if (!jsn_run ("[\"a\r\nb\",\n  1]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp then_comment[] = {
    JSN_T (xtree_json_token_STRING,  "a\\n", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "// c",     1, 2),
  };
  if (!jsn_run ("\"a\n\" // c", then_comment, JSN_N (then_comment)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_symbols_single
static int test_json_scan_symbols_single (void)
{
  // Each input is one SYMBOL whose text is the input itself. A symbol is a
  // run of characters from the symbol set (letters, digits and
  // !@#$%^&*()_+=-;><?.'\ ; note: no slash) that does not start with a number character
  // (digit, '+', '-' or '.') and is not exactly one of the keyword tokens.
  // Words that merely start like a keyword ("truex") are symbols; words
  // that start with a number character are NUMBERs (see
  // test_json_scan_numbers_malformed). A colon is NOT in the symbol set.
  // Characters outside the set are UNRECOGNISED (see
  // test_json_scan_unrecognised_*).
  static const char *const cases[] = {
    /* ordinary identifiers */
    "x", "SomeVar", "some_var", "_x", "x1", "ONE", "TWO)", "some-name",
    "a.b.c", "$", "$(ONE)",
    /* number characters are fine after the first character */
    "x-1", "a+b", "a.5", "e5", "_1", "x1e5", "v1.0", "a-", "a+", "a.",
    "e+5", "b0x1f",
    /* punctuation and operators that are not JSON */
    "@", "#", "'x'", "<", "=", ";", "(", "\\", "*",
    /* (a slash is not a symbol character: see the unrecognised tests) */
    /* keywords with the wrong case, or cut short, or extended */
    "True", "TRUE", "False", "FALSE", "Null", "NULL",
    "t", "tr", "tru", "f", "fals", "n", "nul", "nil",
    "trux", "trUe", "fAlse", "nulL",
    "truex", "falsey", "nullable", "trueish", "nullx",
    "Undefined", "UNDEFINED", "undefinedx",
    "infinity", "INFINITY", "nan", "NAN", "Nan",
    /* every punctuation character in the symbol set, at the start */
    "!", "%", "^", "&", ")", ">", "?", "!x", "%x", "^x", "&x", ">x", "?x",
    "!@#$%^&*()_+=-;><?.'\\", "x!@#$%^&*()_+=-;><?.'\\",
  };
  return jsn_run_single_cases (xtree_json_token_SYMBOL, cases, JSN_N (cases));
}

// name: test_json_scan_unrecognised_single
static int test_json_scan_unrecognised_single (void)
{
  // Characters that are not whitespace, not JSON punctuation, not a
  // double quote and not in the symbol set (so they cannot start a
  // number, string or symbol) are UNRECOGNISED. A run of such characters
  // is ASSUMED to be a single token whose text is the whole run (so the
  // two bytes of a UTF-8 "e-acute" are one token). If the library emits
  // one token per byte instead, change the table below.
  //
  // Form feed and vertical tab are deliberately not tested: whether they
  // are whitespace (isspace) or UNRECOGNISED is not yet specified.
  static const char *const cases[] = {
    "|", "~", "`", "|~", "||", "~`|",
    /* a slash is not in the symbol set; a lone "/" (not "//") is a run
     * of one unrecognised character, and joins neighbouring ones */
    "/", "/|", "|/", "|/~", "/\x01",
    "\x01", "\x02\x03", "\x1f", "\x7f", "\x01\x7f",
    "\xc2\xa0",                  /* NBSP */
    "\xc3\xa9",                  /* e-acute */
    "\xff", "\x80", "\xff\xfe",
    "\xe2\x82\xac",              /* euro sign */
    "\xf0\x9f\x98\x80",          /* emoji */
  };
  return jsn_run_single_cases (xtree_json_token_UNRECOGNISED, cases,
                               JSN_N (cases));
}

// name: test_json_scan_unrecognised_delimited
static int test_json_scan_unrecognised_delimited (void)
{
  // An unrecognised run ends at whitespace, at { } [ ] : , at a double
  // quote and at the first character of the symbol set; and a symbol
  // ends at the first character that is not in the symbol set.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp sym_pipe_sym[] = {
    JSN_T (xtree_json_token_SYMBOL,       "a", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 1),
    JSN_T (xtree_json_token_SYMBOL,       "b", 0, 2),
  };
  if (!jsn_run ("a|b", sym_pipe_sym, JSN_N (sym_pipe_sym)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp utf8_tail[] = {
    JSN_T (xtree_json_token_SYMBOL,       "caf",        0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "\xc3\xa9",   0, 3),
  };
  if (!jsn_run ("caf\xc3\xa9", utf8_tail, JSN_N (utf8_tail)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp utf8_mid[] = {
    JSN_T (xtree_json_token_SYMBOL,       "na",         0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "\xc3\xaf",   0, 2),
    JSN_T (xtree_json_token_SYMBOL,       "ve",         0, 4),
  };
  if (!jsn_run ("na\xc3\xaf" "ve", utf8_mid, JSN_N (utf8_mid)))
    rc = EXIT_FAILURE;

  // A slash is outside the symbol set, so it splits symbols and is
  // UNRECOGNISED.
  static const struct jsn_exp slash_mid[] = {
    JSN_T (xtree_json_token_SYMBOL,       "a", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "/", 0, 1),
    JSN_T (xtree_json_token_SYMBOL,       "b", 0, 2),
  };
  if (!jsn_run ("a/b", slash_mid, JSN_N (slash_mid)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp slash_start[] = {
    JSN_T (xtree_json_token_UNRECOGNISED, "/", 0, 0),
    JSN_T (xtree_json_token_SYMBOL,       "x", 0, 1),
  };
  if (!jsn_run ("/x", slash_start, JSN_N (slash_start)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp slash_star[] = {
    JSN_T (xtree_json_token_UNRECOGNISED, "/", 0, 0),
    JSN_T (xtree_json_token_SYMBOL,       "*", 0, 1),
  };
  if (!jsn_run ("/*", slash_star, JSN_N (slash_star)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp star_slash[] = {
    JSN_T (xtree_json_token_SYMBOL,       "*", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "/", 0, 1),
  };
  if (!jsn_run ("*/", star_slash, JSN_N (star_slash)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "~", 0, 2),
    JSN_T (xtree_json_token_SYMBOL,       "x", 0, 4),
  };
  if (!jsn_run ("| ~ x", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp structure[] = {
    JSN_T (xtree_json_token_LBRACE,       "{", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 1),
    JSN_T (xtree_json_token_COLON,        ":", 0, 2),
    JSN_T (xtree_json_token_UNRECOGNISED, "~", 0, 3),
    JSN_T (xtree_json_token_COMMA,        ",", 0, 4),
    JSN_T (xtree_json_token_UNRECOGNISED, "`", 0, 5),
    JSN_T (xtree_json_token_RBRACE,       "}", 0, 6),
  };
  if (!jsn_run ("{|:~,`}", structure, JSN_N (structure)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp brackets[] = {
    JSN_T (xtree_json_token_LBRACKET,     "[", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 1),
    JSN_T (xtree_json_token_RBRACKET,     "]", 0, 2),
    JSN_T (xtree_json_token_UNRECOGNISED, "~", 0, 3),
  };
  if (!jsn_run ("[|]~", brackets, JSN_N (brackets)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp quoted[] = {
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 0),
    JSN_T (xtree_json_token_STRING,       "s", 0, 1),
    JSN_T (xtree_json_token_UNRECOGNISED, "~", 0, 4),
  };
  if (!jsn_run ("|\"s\"~", quoted, JSN_N (quoted)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp lines[] = {
    JSN_T (xtree_json_token_UNRECOGNISED, "|", 0, 0),
    JSN_T (xtree_json_token_UNRECOGNISED, "~", 1, 0),
  };
  if (!jsn_run ("|\n~", lines, JSN_N (lines)))
    rc = EXIT_FAILURE;

  // Unrecognised characters are not part of a string, and a string may
  // contain them.
  static const struct jsn_exp in_string[] = {
    JSN_T (xtree_json_token_STRING, "|~\xc3\xa9", 0, 0),
  };
  if (!jsn_run ("\"|~\xc3\xa9\"", in_string, JSN_N (in_string)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_symbols_delimited
static int test_json_scan_symbols_delimited (void)
{
  // A symbol ends at the first character outside the symbol set: whitespace,
  // { } [ ] : , a double quote, or any unrecognised character. Symbols
  // therefore never swallow JSON structure.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp parens[] = {
    JSN_T (xtree_json_token_SYMBOL, "$(ONE", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "TWO)",  0, 6),
  };
  if (!jsn_run ("$(ONE TWO)", parens, JSN_N (parens)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp brace[] = {
    JSN_T (xtree_json_token_SYMBOL, "ONE", 0, 0),
    JSN_T (xtree_json_token_LBRACE, "{",   0, 3),
    JSN_T (xtree_json_token_SYMBOL, "TWO", 0, 4),
  };
  if (!jsn_run ("ONE{TWO", brace, JSN_N (brace)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp bracket[] = {
    JSN_T (xtree_json_token_SYMBOL,   "a", 0, 0),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 1),
    JSN_T (xtree_json_token_SYMBOL,   "b", 0, 2),
  };
  if (!jsn_run ("a]b", bracket, JSN_N (bracket)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp unquoted_key[] = {
    JSN_T (xtree_json_token_LBRACE, "{",   0, 0),
    JSN_T (xtree_json_token_SYMBOL, "foo", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",   0, 4),
    JSN_T (xtree_json_token_NUMBER, "1",   0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",   0, 6),
  };
  if (!jsn_run ("{foo:1}", unquoted_key, JSN_N (unquoted_key)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp list[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_SYMBOL,   "A", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_SYMBOL,   "B", 0, 3),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 4),
  };
  if (!jsn_run ("[A,B]", list, JSN_N (list)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_LBRACE,   "{",       0, 0),
    JSN_T (xtree_json_token_SYMBOL,   "SomeVar", 0, 2),
    JSN_T (xtree_json_token_COLON,    ":",       0, 10),
    JSN_T (xtree_json_token_LBRACKET, "[",       0, 12),
    JSN_T (xtree_json_token_SYMBOL,   "a",       0, 14),
    JSN_T (xtree_json_token_COMMA,    ",",       0, 16),
    JSN_T (xtree_json_token_SYMBOL,   "b",       0, 18),
    JSN_T (xtree_json_token_RBRACKET, "]",       0, 20),
    JSN_T (xtree_json_token_RBRACE,   "}",       0, 22),
  };
  if (!jsn_run ("{ SomeVar : [ a , b ] }", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp key_value[] = {
    JSN_T (xtree_json_token_SYMBOL, "key",   0, 0),
    JSN_T (xtree_json_token_COLON,  ":",     0, 3),
    JSN_T (xtree_json_token_SYMBOL, "value", 0, 4),
  };
  if (!jsn_run ("key:value", key_value, JSN_N (key_value)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp comma[] = {
    JSN_T (xtree_json_token_SYMBOL, "SomeVar", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",       0, 7),
    JSN_T (xtree_json_token_SYMBOL, "Other",   0, 8),
  };
  if (!jsn_run ("SomeVar,Other", comma, JSN_N (comma)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp quote[] = {
    JSN_T (xtree_json_token_SYMBOL, "a",     0, 0),
    JSN_T (xtree_json_token_STRING, "b", 0, 1),
  };
  if (!jsn_run ("a\"b\"", quote, JSN_N (quote)))
    rc = EXIT_FAILURE;

  // A colon is not a symbol character: it always ends a symbol and is its
  // own token, wherever it appears.
  static const struct jsn_exp colon_start[] = {
    JSN_T (xtree_json_token_COLON,  ":", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "a", 0, 1),
  };
  if (!jsn_run (":a", colon_start, JSN_N (colon_start)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp colon_end[] = {
    JSN_T (xtree_json_token_SYMBOL, "a", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":", 0, 1),
  };
  if (!jsn_run ("a:", colon_end, JSN_N (colon_end)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp colon_namespace[] = {
    JSN_T (xtree_json_token_SYMBOL, "ns",  0, 0),
    JSN_T (xtree_json_token_COLON,  ":",   0, 2),
    JSN_T (xtree_json_token_SYMBOL, "tag", 0, 3),
    JSN_T (xtree_json_token_COLON,  ":",   0, 6),
    JSN_T (xtree_json_token_SYMBOL, "x",   0, 7),
  };
  if (!jsn_run ("ns:tag:x", colon_namespace, JSN_N (colon_namespace)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp colon_double[] = {
    JSN_T (xtree_json_token_SYMBOL, "a", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":", 0, 2),
    JSN_T (xtree_json_token_SYMBOL, "b", 0, 3),
  };
  if (!jsn_run ("a::b", colon_double, JSN_N (colon_double)))
    rc = EXIT_FAILURE;

  // Other punctuation in the symbol set stays inside the symbol.
  static const struct jsn_exp punct[] = {
    JSN_T (xtree_json_token_SYMBOL, "<tag>", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":",     0, 5),
    JSN_T (xtree_json_token_SYMBOL, "$(FOO)", 0, 6),
  };
  if (!jsn_run ("<tag>:$(FOO)", punct, JSN_N (punct)))
    rc = EXIT_FAILURE;

  // A slash is not a symbol character, so "//" ends a symbol and starts a
  // comment.
  static const struct jsn_exp sym_comment[] = {
    JSN_T (xtree_json_token_SYMBOL,  "a",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//b", 0, 1),
  };
  if (!jsn_run ("a//b", sym_comment, JSN_N (sym_comment)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp url[] = {
    JSN_T (xtree_json_token_SYMBOL,  "http",    0, 0),
    JSN_T (xtree_json_token_COLON,   ":",       0, 4),
    JSN_T (xtree_json_token_COMMENT, "//x.com", 0, 5),
  };
  if (!jsn_run ("http://x.com", url, JSN_N (url)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp lines[] = {
    JSN_T (xtree_json_token_SYMBOL, "x", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "y", 1, 0),
  };
  if (!jsn_run ("x\ny", lines, JSN_N (lines)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_special_values
static int test_json_scan_special_values (void)
{
  int rc = EXIT_SUCCESS;

  static const char *const pos_inf[] = { "Infinity" };
  static const char *const nan[]     = { "NaN" };
  static const char *const undef[]   = { "undefined" };

  if (jsn_run_single_cases (xtree_json_token_INFINITY, pos_inf,
                            JSN_N (pos_inf)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_NAN, nan,
                            JSN_N (nan)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_UNDEFINED, undef,
                            JSN_N (undef)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;

  // Infinity and NaN are keywords only when they are the whole word, just
  // like true, false, null and undefined: anything suffixed onto them makes
  // the word a SYMBOL with the full text. A sign in front is a separate
  // NUMBER token; see test_json_scan_numbers_malformed.
  static const char *const pos_inf_sfx[] = {
    "Infinity2", "Infinityx", "InfinityX", "Infinity-", "Infinity+",
    "Infinity.", "Infinitya", "InfinityF", "Infinityb", "InfinityB",
    "Infinitye5", "Infinity0x1f", "Infinity1e+10", "Infinity..",
    "Infinity-2", "Infinity2026-10-25", "InfinityfedcbaFEDCBA",
  };
  static const char *const nan_sfx[] = {
    "NaN2", "NaNx", "NaNX", "NaN-", "NaN+", "NaN.", "NaNa", "NaNF", "NaNb",
    "NaNB", "NaNe5", "NaN0x1f", "NaN1e+10", "NaN..", "NaN-2",
    "NaN2026-10-25", "NaNfedcbaFEDCBA",
  };
  if (jsn_run_single_cases (xtree_json_token_SYMBOL, pos_inf_sfx,
                            JSN_N (pos_inf_sfx)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_SYMBOL, nan_sfx,
                            JSN_N (nan_sfx)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;

  static const struct jsn_exp seq[] = {
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 0),
    JSN_T (xtree_json_token_NUMBER,       "-",         0, 9),
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 10),
    JSN_T (xtree_json_token_NUMBER,       "+",         0, 19),
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 20),
    JSN_T (xtree_json_token_NAN,          "NaN",       0, 29),
    JSN_T (xtree_json_token_UNDEFINED,    "undefined", 0, 33),
  };
  if (!jsn_run ("Infinity -Infinity +Infinity NaN undefined",
                seq, JSN_N (seq)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET,     "[",         0, 0),
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 1),
    JSN_T (xtree_json_token_COMMA,        ",",         0, 9),
    JSN_T (xtree_json_token_NUMBER,       "-",         0, 10),
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 11),
    JSN_T (xtree_json_token_COMMA,        ",",         0, 19),
    JSN_T (xtree_json_token_NAN,          "NaN",       0, 20),
    JSN_T (xtree_json_token_RBRACKET,     "]",         0, 23),
  };
  if (!jsn_run ("[Infinity,-Infinity,NaN]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_object[] = {
    JSN_T (xtree_json_token_LBRACE,    "{",         0, 0),
    JSN_T (xtree_json_token_STRING,    "a",     0, 1),
    JSN_T (xtree_json_token_COLON,     ":",         0, 4),
    JSN_T (xtree_json_token_UNDEFINED, "undefined", 0, 5),
    JSN_T (xtree_json_token_RBRACE,    "}",         0, 14),
  };
  if (!jsn_run ("{\"a\":undefined}", in_object, JSN_N (in_object)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_comments_single
static int test_json_scan_comments_single (void)
{
  // Each input is one COMMENT whose text is the input itself: everything
  // after "//" is comment, whatever it contains.
  static const char *const cases[] = {
    "//", "// hello", "//x", "///", "///x", "// // nested", "//a//b",
    "// \"quoted\"", "// \"unterminated string",
    "// {}[]:,", "// a/*b*/", "//\ttab", "// \xc3\xa9", "// 1 2 3 true",
    "// http://x.com", "// 'x", "// \\", "// \\n",
  };
  return jsn_run_single_cases (xtree_json_token_COMMENT, cases,
                               JSN_N (cases));
}

// name: test_json_scan_comments_in_context
static int test_json_scan_comments_in_context (void)
{
  int rc = EXIT_SUCCESS;

  // A comment ends at LF; the LF is not part of it.
  static const struct jsn_exp lf[] = {
    JSN_T (xtree_json_token_COMMENT, "// hi", 0, 0),
  };
  if (!jsn_run ("// hi\n", lf, JSN_N (lf)))
    rc = EXIT_FAILURE;

  // CRLF: neither the CR nor the LF is part of the comment.
  static const struct jsn_exp crlf[] = {
    JSN_T (xtree_json_token_COMMENT, "// hi", 0, 0),
    JSN_T (xtree_json_token_NUMBER,  "1",     1, 0),
  };
  if (!jsn_run ("// hi\r\n1", crlf, JSN_N (crlf)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp trailing[] = {
    JSN_T (xtree_json_token_NUMBER,  "1",      0, 0),
    JSN_T (xtree_json_token_COMMENT, "// one", 0, 2),
    JSN_T (xtree_json_token_NUMBER,  "2",      1, 0),
    JSN_T (xtree_json_token_COMMENT, "// two", 1, 2),
  };
  if (!jsn_run ("1 // one\n2 // two\n", trailing, JSN_N (trailing)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",          0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1",          0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",          0, 2),
    JSN_T (xtree_json_token_COMMENT,  "// first",   0, 4),
    JSN_T (xtree_json_token_NUMBER,   "2",          1, 1),
    JSN_T (xtree_json_token_RBRACKET, "]",          1, 2),
  };
  if (!jsn_run ("[1, // first\n 2]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  // The comment starts right after the previous token, with no space.
  // (Not after a number: "1//c" is the single NUMBER "1//c".)
  static const struct jsn_exp after_number[] = {
    JSN_T (xtree_json_token_NUMBER,  "1",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c", 0, 2),
  };
  if (!jsn_run ("1 //c", after_number, JSN_N (after_number)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_comma[] = {
    JSN_T (xtree_json_token_NUMBER,  "1",   0, 0),
    JSN_T (xtree_json_token_COMMA,   ",",   0, 1),
    JSN_T (xtree_json_token_COMMENT, "//c", 0, 2),
  };
  if (!jsn_run ("1,//c", after_comma, JSN_N (after_comma)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp number_swallows[] = {
    JSN_T (xtree_json_token_NUMBER, "1//c", 0, 0),
  };
  if (!jsn_run ("1//c", number_swallows, JSN_N (number_swallows)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_literal[] = {
    JSN_T (xtree_json_token_TRUE,    "true", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c",  0, 4),
  };
  if (!jsn_run ("true//c", after_literal, JSN_N (after_literal)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_special[] = {
    JSN_T (xtree_json_token_INFINITY, "Infinity", 0, 0),
    JSN_T (xtree_json_token_COMMENT,  "//c",      0, 8),
  };
  if (!jsn_run ("Infinity//c", after_special, JSN_N (after_special)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_string[] = {
    JSN_T (xtree_json_token_STRING,  "s", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c",   0, 3),
  };
  if (!jsn_run ("\"s\"//c", after_string, JSN_N (after_string)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_brace[] = {
    JSN_T (xtree_json_token_LBRACE,  "{",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c", 0, 1),
    JSN_T (xtree_json_token_RBRACE,  "}",   1, 0),
  };
  if (!jsn_run ("{//c\n}", after_brace, JSN_N (after_brace)))
    rc = EXIT_FAILURE;

  // Consecutive comments are separate tokens, and blank lines are skipped.
  static const struct jsn_exp consecutive[] = {
    JSN_T (xtree_json_token_COMMENT, "//a", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//b", 1, 0),
  };
  if (!jsn_run ("//a\n//b", consecutive, JSN_N (consecutive)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp blank_lines[] = {
    JSN_T (xtree_json_token_COMMENT, "// a", 0, 0),
    JSN_T (xtree_json_token_NUMBER,  "1",    3, 0),
  };
  if (!jsn_run ("// a\n\n\n1", blank_lines, JSN_N (blank_lines)))
    rc = EXIT_FAILURE;

  // Commented-out JSON is just comment text.
  static const struct jsn_exp commented_out[] = {
    JSN_T (xtree_json_token_LBRACE,  "{",                    0, 0),
    JSN_T (xtree_json_token_COMMENT, "// \"a\": 1, \"b\": [", 1, 2),
    JSN_T (xtree_json_token_RBRACE,  "}",                    2, 0),
  };
  if (!jsn_run ("{\n  // \"a\": 1, \"b\": [\n}", commented_out,
                JSN_N (commented_out)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_error_unterminated_string
static int test_json_scan_error_unterminated_string (void)
{
  // The only way scanning fails. The ERROR token is positioned at the end
  // of the input, where the closing quote was expected. Strings may span
  // lines, so a newline does not end an unterminated string.
  static const struct jsn_bad_case cases[] = {
    { "\"", 0, 1 },
    { "\"abc", 0, 4 },
    { "\"abc\\", 0, 5 },             /* input ends after a backslash */
    { "\"abc\\\"", 0, 6 },           /* the only closing quote is escaped */
    { "\"\\u12", 0, 5 },
    { "\"// comment", 0, 11 },       /* not a comment inside a string */
    { "\"abc\n", 1, 0 },
    { "\"a\nb", 1, 1 },
    { "\"a\r\nb", 1, 1 },
    { "\"\n\n", 2, 0 },
    { "\"line one\nline two", 1, 8 },
  };
  return jsn_run_bad_cases (cases, JSN_N (cases));
}

// name: test_json_scan_error_after_tokens
static int test_json_scan_error_after_tokens (void)
{
  // Valid tokens are returned (with correct positions) up to the point
  // of the error.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp in_object[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "a", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",     0, 4),
    JSN_T (xtree_json_token_ERROR,  "",      0, 7),
  };
  if (!jsn_run ("{\"a\":\"b", in_object, JSN_N (in_object)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp multi_line[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_ERROR,    "",  2, 3),
  };
  if (!jsn_run ("[1,\n\"abc\ndef", multi_line, JSN_N (multi_line)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_symbol[] = {
    JSN_T (xtree_json_token_SYMBOL, "x", 0, 0),
    JSN_T (xtree_json_token_ERROR,  "",  0, 3),
  };
  if (!jsn_run ("x \"", after_symbol, JSN_N (after_symbol)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_comment[] = {
    JSN_T (xtree_json_token_COMMENT, "// c", 0, 0),
    JSN_T (xtree_json_token_ERROR,   "",     1, 1),
  };
  if (!jsn_run ("// c\n\"", after_comment, JSN_N (after_comment)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_long_symbol_and_comment
static int test_json_scan_long_symbol_and_comment (void)
{
  int ret = EXIT_FAILURE;
  const size_t len = 100000;
  char *sym = malloc (len + 1);
  char *com = malloc (len + 3);

  if (!sym || !com) {
    PERROR ("Failed to allocate test data\n");
    goto cleanup;
  }

  for (size_t i=0; i<len; i++)
    sym[i] = (char)('A' + (i % 26));
  sym[len] = '\0';

  com[0] = '/';
  com[1] = '/';
  for (size_t i=0; i<len; i++)
    com[i + 2] = (char)('a' + (i % 26));
  com[len + 2] = '\0';

  struct jsn_exp e = JSN_T (xtree_json_token_SYMBOL, sym, 0, 0);
  if (!jsn_run (sym, &e, 1))
    goto cleanup;
  e.id = xtree_json_token_COMMENT;
  e.text = com;
  if (!jsn_run (com, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (sym);
  free (com);
  return ret;
}

// name: test_json_scan_null_reader
static int test_json_scan_null_reader (void)
{
  // A NULL reader must be handled without crashing. What is returned is
  // up to the library: either NULL or an ERROR token.
  int ret = EXIT_FAILURE;
  xtree_scan_token_t *tok = xtree_json_scan (NULL);

  if (tok && xtree_scan_token_id (tok) != xtree_json_token_ERROR) {
    PERROR ("Scanning a NULL reader returned a [%s] token\n",
            jsn_idname (xtree_scan_token_id (tok)));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&tok);
  return ret;
}

// name: test_json_scan_tokens_independent
static int test_json_scan_tokens_independent (void)
{
  // Tokens own their data: freeing one must not disturb another, and
  // both must survive the reader being freed.
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = xtree_reader_str (JSN_NAME, U ("\"a\","));
  xtree_scan_token_t *t1 = NULL;
  xtree_scan_token_t *t2 = NULL;

  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  t1 = xtree_json_scan (reader);
  t2 = xtree_json_scan (reader);
  if (!t1 || !t2) {
    PERROR ("Failed to scan two tokens\n");
    goto cleanup;
  }

  xtree_reader_free (&reader);

  const unsigned char *text = xtree_scan_token_text (t1);
  if (xtree_scan_token_id (t1) != xtree_json_token_STRING ||
      !text || ustrcmp (text, U ("a")) != 0) {
    PERROR ("First token is wrong after the reader was freed\n");
    goto cleanup;
  }

  xtree_scan_token_free (&t1);
  t1 = NULL;

  text = xtree_scan_token_text (t2);
  if (xtree_scan_token_id (t2) != xtree_json_token_COMMA ||
      !text || ustrcmp (text, U (",")) != 0) {
    PERROR ("Second token is wrong after the first was deleted\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&t1);
  xtree_scan_token_free (&t2);
  xtree_reader_free (&reader);
  return ret;
}

// name: test_json_scan_long_string
static int test_json_scan_long_string (void)
{
  // Much longer than any plausible internal buffer.
  int ret = EXIT_FAILURE;
  const size_t len = 100000;
  char *buf = malloc (len + 3);
  char *want = NULL;

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[0] = '"';
  for (size_t i=0; i<len; i++)
    buf[i + 1] = (char)('a' + (i % 26));
  buf[len + 1] = '"';
  buf[len + 2] = '\0';

  // The expected token text is the content without the quotes.
  want = malloc (len + 1);
  if (!want) {
    PERROR ("Failed to allocate expected text\n");
    goto cleanup;
  }
  memcpy (want, buf + 1, len);
  want[len] = '\0';

  struct jsn_exp e = JSN_T (xtree_json_token_STRING, want, 0, 0);
  if (!jsn_run (buf, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (want);
  free (buf);
  return ret;
}

// name: test_json_scan_long_number
static int test_json_scan_long_number (void)
{
  int ret = EXIT_FAILURE;
  const size_t ndigits = 5000;
  static const char tail[] = ".5e+10";
  char *buf = malloc (1 + ndigits + sizeof tail);

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[0] = '1';
  for (size_t i=0; i<ndigits; i++)
    buf[i + 1] = (char)('0' + (i % 10));
  memcpy (buf + 1 + ndigits, tail, sizeof tail);   /* includes the NUL */

  struct jsn_exp e = JSN_T (xtree_json_token_NUMBER, buf, 0, 0);
  if (!jsn_run (buf, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (buf);
  return ret;
}

// name: test_json_scan_many_tokens
static int test_json_scan_many_tokens (void)
{
  // "[0,0,0,...,0]": 2 + count numbers + (count - 1) commas tokens.
  int ret = EXIT_FAILURE;
  const size_t count = 5000;
  xtree_reader_t *reader = NULL;
  char *buf = malloc (2 * count + 2);
  size_t n = 0;

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[n++] = '[';
  for (size_t i=0; i<count; i++) {
    if (i)
      buf[n++] = ',';
    buf[n++] = '0';
  }
  buf[n++] = ']';
  buf[n] = '\0';

  reader = xtree_reader_str (JSN_NAME, (const unsigned char *)buf);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  size_t line0 = xtree_reader_line (reader);
  size_t cpos0 = xtree_reader_cpos (reader);

  if (!jsn_check_token (reader, xtree_json_token_LBRACKET, U ("["),
                        line0, cpos0))
    goto cleanup;
  for (size_t i=0; i<count; i++) {
    if (!jsn_check_token (reader, xtree_json_token_NUMBER, U ("0"),
                          line0, cpos0 + 1 + 2 * i)) {
      PERROR ("Number %zu was wrong\n", i);
      goto cleanup;
    }
    if (i + 1 < count &&
        !jsn_check_token (reader, xtree_json_token_COMMA, U (","),
                          line0, cpos0 + 2 + 2 * i)) {
      PERROR ("Comma %zu was wrong\n", i);
      goto cleanup;
    }
  }
  if (!jsn_check_token (reader, xtree_json_token_RBRACKET, U ("]"),
                        line0, cpos0 + 2 * count))
    goto cleanup;
  if (!jsn_check_eof (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  free (buf);
  return ret;
}

/* ------------------------------------------------------------------
 * Tests for xtree_scan_token_dump()
 *
 * The dump is a sequence of `name: value\n` lines, one per token field.
 * The expected strings below are literals. If one of them is wrong, the
 * failure message prints the actual output: check it, and if it is
 * correct, copy it into the literal.
 * ------------------------------------------------------------------ */

/* Dump `tok` into *dst and compare the result with `expected`. Also checks
 * that the return value is the same buffer as *dst. */
static bool tdump_check (const xtree_scan_token_t *tok, unsigned char **dst,
                         const char *expected)
{
  unsigned char *res = xtree_scan_token_dump (tok, dst);

  if (!res || res != *dst) {
    PERROR ("xtree_scan_token_dump returned %p, *dst is %p\n",
            (void *)res, (void *)*dst);
    return false;
  }
  if (strcmp ((const char *)*dst, expected) != 0) {
    PERROR ("Dump does not match expected output\n"
            "--- expected ---\n%s"
            "--- actual ---\n%s",
            expected, (const char *)*dst);
    return false;
  }
  return true;
}

// name: test_scan_token_dump
static int test_scan_token_dump (void)
{
  int ret = EXIT_FAILURE;
  xtree_scan_token_t *tok1 = NULL;
  xtree_scan_token_t *tok2 = NULL;
  unsigned char *dst = NULL;

  static const char *const expected1 =
    "token-id:        42\n"
    "token-text:      [hello]\n"
    "token-filename:  [dump-test.json]\n"
    "token-line:      [7]\n"
    "token-cpos:      [13]";

  static const char *const expected2 =
    "token-id:        -1\n"
    "token-text:      []\n"
    "token-filename:  [other.txt]\n"
    "token-line:      [123]\n"
    "token-cpos:      [456]";

  tok1 = xtree_scan_token_new (42, "dump-test.json", 7, 13, U ("hello"), 5);
  tok2 = xtree_scan_token_new (-1, "other.txt", 123, 456, U (""), 0);
  if (!tok1 || !tok2) {
    PERROR ("Failed to create tokens\n");
    goto cleanup;
  }

  /* Dump into a NULL destination. */
  if (!tdump_check (tok1, &dst, expected1))
    goto cleanup;

  /* Dump into a destination that already holds a dump: it is replaced,
   * not appended to. */
  if (!tdump_check (tok2, &dst, expected2))
    goto cleanup;
  if (!tdump_check (tok1, &dst, expected1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (dst);
  xtree_scan_token_free (&tok1);
  xtree_scan_token_free (&tok2);
  return ret;
}

/* Recursively append a compact representation of `node` to *out (which
 * must be heap-allocated, see ds_str_append()): one line per node, one
 * space of indent per depth level, the node's name in parentheses and, for
 * ATOM nodes, the value in square brackets. Returns false on error. */
static bool nv_dump (struct xtree_errobj_t *err, xtree_node_t *node,
                     size_t depth, char **out)
{
  const unsigned char *name = xtree_node_name_get (node);
  enum xtree_node_type_t type = xtree_node_type_get (node);

  for (size_t i=0; i<depth; i++) {
    if (!ds_str_append (out, " ", NULL))
      return false;
  }
  if (!ds_str_append (out, "(", name ? CS (name) : "", ")", NULL))
    return false;

  if (type == xtree_node_type_ATOM) {
    const unsigned char *value = xtree_node_value_get (err, node);
    if (!ds_str_append (out, " [", value ? CS (value) : "", "]", NULL))
      return false;
  }
  if (!ds_str_append (out, "\n", NULL))
    return false;

  if (type == xtree_node_type_LIST) {
    size_t nchildren = xtree_node_child_count (node);
    for (size_t i=0; i<nchildren; i++) {
      xtree_node_t *child = xtree_node_child_get (err, node, i);
      if (!child || !nv_dump (err, child, depth + 1, out))
        return false;
    }
  }
  return true;
}

/* Produce a compact string representation of a tree, for example
 *
 *   (object)
 *    (field)
 *     (aa) []
 *     (number) [11]
 *
 * Returns a heap-allocated string that the caller must free, or NULL on
 * error. */
static char *node_dump_nv (struct xtree_errobj_t *err, xtree_node_t *node)
{
  char *out = node ? ds_str_dup ("") : NULL;

  if (!out)
    return NULL;
  if (!nv_dump (err, node, 0, &out)) {
    free (out);
    return NULL;
  }
  return out;
}

/* ****************************************************************************
 * The JSON parser is pretty relaxed: allows trailing commas, symbols, etc.
 * See the header json_scan.h.
 * The expected output is produced by node_dump_nv(): only the node name and
 * (for atoms) the node value are shown, with indentation showing the depth.
 * All xtree strings are unsigned char *, hence the casts below.
 */
static int test_json_parse_happy (void)
{
  int ret = EXIT_FAILURE;

  static const unsigned char *obj1 = (const unsigned char *)
    "{\n"
    "  \"aa\": 11,\n"
    "  \"bb\": 22\n"
    "}";

  static const unsigned char *obj1_exp = (const unsigned char *)
    "(object)\n"
    " (field)\n"
    "  (aa) []\n"
    "  (number) [11]\n"
    " (field)\n"
    "  (bb) []\n"
    "  (number) [22]\n";

  static const unsigned char *obj2 = (const unsigned char *)
    "{\n"
    "  \"aa\": 1111,\n"
    "  \"bb\": {\n"
    "    \"cc\": 2222,\n"
    "    \"dd\": 3333,\n"
    "   }\n"
    "}";

  static const unsigned char *obj2_exp = (const unsigned char *)
    "(object)\n"
    " (field)\n"
    "  (aa) []\n"
    "  (number) [1111]\n"
    " (field)\n"
    "  (bb) []\n"
    "  (object)\n"
    "   (field)\n"
    "    (cc) []\n"
    "    (number) [2222]\n"
    "   (field)\n"
    "    (dd) []\n"
    "    (number) [3333]\n";

  static const unsigned char *obj3 = (const unsigned char *)
    "{\n"
    "  \"aaa\": 777,\n"
    "  \"bbb\": {\n"
    "    \"ccc\": \"some-string-1\",\n"
    "    \"ddd\": true,\n"
    "   },"
    "  \"eee\": symbol-1,\n"
    "  \"fff\": -12.55,\n"
    "}";

  static const unsigned char *obj3_exp = (const unsigned char *)
    "(object)\n"
    " (field)\n"
    "  (aaa) []\n"
    "  (number) [777]\n"
    " (field)\n"
    "  (bbb) []\n"
    "  (object)\n"
    "   (field)\n"
    "    (ccc) []\n"
    "    (string) [some-string-1]\n"
    "   (field)\n"
    "    (ddd) []\n"
    "    (bool) [true]\n"
    " (field)\n"
    "  (eee) []\n"
    "  (symbol) [symbol-1]\n"
    " (field)\n"
    "  (fff) []\n"
    "  (number) [-12.55]\n";

  static const unsigned char *array1 = (const unsigned char *)
    "[\n"
    "  \"aa\", true,\n"
    "  \"bb\", false\n"
    "]";

  static const unsigned char *array1_exp = (const unsigned char *)
    "(array)\n"
    " (string) [aa]\n"
    " (bool) [true]\n"
    " (string) [bb]\n"
    " (bool) [false]\n";

  static const unsigned char *array2 = (const unsigned char *)
    "[\n"
    "  \"a2-1\", NaN,\n"
    "  \"a2-2\", {\n"
    "     \"a2-2-1\": undefined,\n"
    "     \"a2-2-2\": [ A, B, C, D ],\n"
    "   }\n"
    // TODO: This will be suppored in a future version
    // "  // Comments are allowed...\n"
    "  \"a2-3\", \"a2-3 text\",\n"
    "]";

  static const unsigned char *array2_exp = (const unsigned char *)
    "(array)\n"
    " (string) [a2-1]\n"
    " (number) [NaN]\n"
    " (string) [a2-2]\n"
    " (object)\n"
    "  (field)\n"
    "   (a2-2-1) []\n"
    "   (reference) [undefined]\n"
    "  (field)\n"
    "   (a2-2-2) []\n"
    "   (array)\n"
    "    (symbol) [A]\n"
    "    (symbol) [B]\n"
    "    (symbol) [C]\n"
    "    (symbol) [D]\n"
    " (string) [a2-3]\n"
    " (string) [a2-3 text]\n";

  const struct {
    const char *name;
    const unsigned char *input;
    const unsigned char *exp;
  } tests[] = {
    { "happy object1", obj1,   obj1_exp   },
    { "happy object2", obj2,   obj2_exp   },
    { "happy object3", obj3,   obj3_exp   },
    { "happy array1",  array1, array1_exp },
    { "happy array2",  array2, array2_exp },
  };
  size_t ntests = sizeof tests / sizeof tests[0];

  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;
  char *tmp = NULL;

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  for (size_t i=0; i<ntests; i++) {
    xtree_reader_free (&reader);
    xtree_node_free (&node);
    if (!(reader = xtree_reader_str (tests[i].name, tests[i].input))) {
      PERROR ("[%s]: Failed to create reader\n", tests[i].name);
      goto cleanup;
    }
    if (!(node = xtree_json_parse (&err, reader, 2000))) {
      PERROR ("[%s]: Failed to parse\n", tests[i].name);
      goto cleanup;
    }
    free (tmp);
    if (!(tmp = node_dump_nv (&err, node))) {
      PERROR ("[%s]: Failed to dump\n", tests[i].name);
      goto cleanup;
    }
    if (ustrcmp (U (tmp), tests[i].exp) != 0) {
      PERROR ("[%s]: Mismatch\n--- expected ---\n%s--- actual ---\n%s",
              tests[i].name, CS (tests[i].exp), tmp);
      goto cleanup;
    }
  }

  ret = EXIT_SUCCESS;

cleanup:
  if (err.syscode || err.libcode) {
    PERROR ("Error: libcode=%i syscode=%i\n"
            "libmsg=[%s]\n"
            "sysmsg=[%s]\n"
            "src=[%s:%i]\n"
            "message=%zu bytes\n"
            "%s\n",
            err.libcode, err.syscode,
            err.libmsg, err.sysmsg,
            err.srcfile, err.srcline,
            err.message_len,
            err.message && err.message[0] ? err.message : "(No message)");
  }
  xtree_errobj_reset (&err, 0);
  free (tmp);
  xtree_node_free (&node);
  xtree_reader_free (&reader);
  return ret;
}


/* ------------------------------------------------------------------
 * Edge-case tests for xtree_json_parse()
 *
 * Dialect (taken from xtree_json_scan.h and test_json_parse_happy):
 *   - Strings: quotes stripped, escapes NOT decoded, may span lines (LF
 *     becomes the two characters \n, CR becomes \r).
 *   - Numbers are raw text (hex 0x, binary 0b, octal 0NN, ".5", "5.",
 *     leading +). Neither the scanner nor the parser validates or
 *     converts them: the tree is a lingua franca, so every NUMBER token
 *     is stored verbatim as a (number) node, however malformed
 *     ("1abc", "12..13", "0x", "08", "-", ...). Consumers decide what
 *     a number means.
 *   - NaN is a (number); undefined is a (reference); true/false are
 *     (bool); unquoted words are (symbol).
 *   - Trailing commas are accepted.
 *   - Comments are NOT supported yet (planned): see
 *     test_json_parse_comments_unsupported, which is not registered.
 *
 * Tests whose behaviour the headers do not pin down are kept out of the
 * pass/fail list: see test_json_parse_probe (prints, never fails).
 *
 * Every failure case checks: NULL result, error object set (libcode or
 * syscode) and well-formed. Run under ASan/valgrind too, so that the
 * error paths are checked for leaks and double frees.
 * ------------------------------------------------------------------ */

#define JP_NAME "jp-test"

struct jp_ok_case {
  const char *input;
  const char *exp;     /* node_dump_nv() output, or NULL: only check success */
};

/* Parse `input`. On return *node is NULL if the parse failed. Returns
 * false only if the test machinery itself failed. */
static bool jp_parse (const char *input, struct xtree_errobj_t *err,
                      xtree_reader_t **reader, xtree_node_t **node,
                      size_t maxdepth)
{
  if (!xtree_errobj_reset (err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    return false;
  }
  *reader = xtree_reader_str (JP_NAME, (const unsigned char *)input);
  if (!*reader) {
    PERROR ("Failed to create reader\n");
    return false;
  }
  *node = xtree_json_parse (err, *reader, maxdepth);
  return true;
}

static void jp_cleanup (struct xtree_errobj_t *err, xtree_reader_t **reader,
                        xtree_node_t **node)
{
  xtree_node_free (node);
  xtree_reader_free (reader);
  xtree_errobj_reset (err, 0);
}

/* Parse must succeed; if exp is not NULL the dump must match it. */
static bool jp_expect_ok (const char *input, const char *exp)
{
  bool ok = false;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;
  char *dump = NULL;

  jsn_set_input (input);
  if (!jp_parse (input, &err, &reader, &node, 2000))
    goto cleanup;
  if (!node) {
    PERROR ("Input [%.100s]: expected success, parse failed\n", jsn_cur_input);
    rdr_print_errobj (&err);
    goto cleanup;
  }
  if (exp) {
    dump = node_dump_nv (&err, node);
    if (!dump) {
      PERROR ("Input [%.100s]: failed to dump tree\n", jsn_cur_input);
      goto cleanup;
    }
    if (strcmp (dump, exp) != 0) {
      PERROR ("Input [%.100s]: mismatch\n--- expected ---\n%s"
              "--- actual ---\n%s", jsn_cur_input, exp, dump);
      goto cleanup;
    }
  }
  ok = true;

cleanup:
  free (dump);
  jp_cleanup (&err, &reader, &node);
  return ok;
}

/* Parse must fail cleanly: NULL result, well-formed error object set. */
static bool jp_expect_fail (const char *input)
{
  bool ok = false;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;

  jsn_set_input (input);
  if (!jp_parse (input, &err, &reader, &node, 2000))
    goto cleanup;
  if (node) {
    char *dump = node_dump_nv (&err, node);
    PERROR ("Input [%.100s]: expected failure, but parsed to:\n%s",
            jsn_cur_input, dump ? dump : "(dump failed)\n");
    free (dump);
    goto cleanup;
  }
  if (!err.libcode && !err.syscode) {
    PERROR ("Input [%.100s]: parse returned NULL without setting the "
            "error object\n", jsn_cur_input);
    goto cleanup;
  }
  if (!rdr_expect_wellformed_errobj (&err))
    goto cleanup;
  ok = true;

cleanup:
  jp_cleanup (&err, &reader, &node);
  return ok;
}

static int jp_run_ok_cases (const struct jp_ok_case *c, size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    if (!jp_expect_ok (c[i].input, c[i].exp))
      rc = EXIT_FAILURE;
  }
  return rc;
}

static int jp_run_fail_cases (const char *const *c, size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    if (!jp_expect_fail (c[i]))
      rc = EXIT_FAILURE;
  }
  return rc;
}

// name: test_json_parse_empty_containers
static int test_json_parse_empty_containers (void)
{
  static const struct jp_ok_case cases[] = {
    { "{}",            "(object)\n" },
    { "[]",            "(array)\n" },
    { " \t\r\n{ \n}\r\n ", "(object)\n" },
    { " [ \n ] ",      "(array)\n" },
    { "[[]]",          "(array)\n (array)\n" },
    { "[{}]",          "(array)\n (object)\n" },
    { "[[],{}]",       "(array)\n (array)\n (object)\n" },
    { "[[[[]]]]",      "(array)\n (array)\n  (array)\n   (array)\n" },
    { "{\"a\":{}}",
      "(object)\n (field)\n  (a) []\n  (object)\n" },
    { "{\"a\":[],\"b\":{}}",
      "(object)\n"
      " (field)\n  (a) []\n  (array)\n"
      " (field)\n  (b) []\n  (object)\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_trailing_commas
static int test_json_parse_trailing_commas (void)
{
  static const struct jp_ok_case cases[] = {
    { "[1,]",          "(array)\n (number) [1]\n" },
    { "[1,2,]",        "(array)\n (number) [1]\n (number) [2]\n" },
    { "[\n 1,\n 2,\n]", "(array)\n (number) [1]\n (number) [2]\n" },
    { "[[1,],]",       "(array)\n (array)\n  (number) [1]\n" },
    { "{\"a\":1,}",    "(object)\n (field)\n  (a) []\n  (number) [1]\n" },
    { "{\"a\":{\"b\":1,},}",
      "(object)\n (field)\n  (a) []\n  (object)\n   (field)\n    (b) []\n"
      "    (number) [1]\n" },
    { "[ 1 , ]",       "(array)\n (number) [1]\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_string_edge_cases
static int test_json_parse_string_edge_cases (void)
{
  /* Strings come back raw: no quotes, escapes untouched, newlines in the
   * source rewritten to \n / \r by the scanner. The expected dumps below
   * contain the backslash characters literally (so no real newline ever
   * ends up in a dump line). */
  static const struct jp_ok_case cases[] = {
    { "[\"\"]",              "(array)\n (string) []\n" },
    { "[\" \"]",             "(array)\n (string) [ ]\n" },
    { "[\"a\\nb\"]",         "(array)\n (string) [a\\nb]\n" },     /* escape kept */
    { "[\"a\nb\"]",          "(array)\n (string) [a\\nb]\n" },     /* LF rewritten */
    { "[\"a\r\nb\"]",        "(array)\n (string) [a\\r\\nb]\n" },  /* CRLF rewritten */
    { "[\"\n\n\"]",          "(array)\n (string) [\\n\\n]\n" },
    { "[\"a\\\"b\"]",        "(array)\n (string) [a\\\"b]\n" },    /* escaped quote */
    { "[\"a\\\\\"]",         "(array)\n (string) [a\\\\]\n" },     /* escaped backslash */
    { "[\"\\u12G4 \\q \\x\"]", "(array)\n (string) [\\u12G4 \\q \\x]\n" }, /* invalid escapes ok */
    { "[\"{}[]:,\"]",        "(array)\n (string) [{}[]:,]\n" },
    { "[\"// not a comment\"]", "(array)\n (string) [// not a comment]\n" },
    { "[\"true false null 12\"]", "(array)\n (string) [true false null 12]\n" },
    { "[\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"]",
      "(array)\n (string) [\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80]\n" },
    { "[\"a\tb\"]",          "(array)\n (string) [a\tb]\n" },
    { "[\"a\",\"b\"]",       "(array)\n (string) [a]\n (string) [b]\n" },
    /* keys may be any string, including empty and punctuation-heavy ones */
    { "{\"\":1}",
      "(object)\n (field)\n  () []\n  (number) [1]\n" },
    { "{\"a b:c,d\":1}",
      "(object)\n (field)\n  (a b:c,d) []\n  (number) [1]\n" },
    { "{\"k\\n\":1}",
      "(object)\n (field)\n  (k\\n) []\n  (number) [1]\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_number_forms
static int test_json_parse_number_forms (void)
{
  /* All of these are valid numbers per the scanner header, and the raw
   * text must survive unchanged. */
  static const struct jp_ok_case cases[] = {
    { "[0]",     "(array)\n (number) [0]\n" },
    { "[-0]",    "(array)\n (number) [-0]\n" },
    { "[+1]",    "(array)\n (number) [+1]\n" },
    { "[.5]",    "(array)\n (number) [.5]\n" },
    { "[5.]",    "(array)\n (number) [5.]\n" },
    { "[-.5]",   "(array)\n (number) [-.5]\n" },
    { "[1e+3]",  "(array)\n (number) [1e+3]\n" },
    { "[1E-3]",  "(array)\n (number) [1E-3]\n" },
    { "[-1.5E-3]", "(array)\n (number) [-1.5E-3]\n" },
    { "[0x1F]",  "(array)\n (number) [0x1F]\n" },
    { "[0XfF]",  "(array)\n (number) [0XfF]\n" },
    { "[0b101]", "(array)\n (number) [0b101]\n" },
    { "[0B1]",   "(array)\n (number) [0B1]\n" },
    { "[017]",   "(array)\n (number) [017]\n" },
    { "[12345678901234567890123456789012345678901234567890]",
      "(array)\n (number) [12345678901234567890123456789012345678901234567890]\n" },
    { "[NaN]",   "(array)\n (number) [NaN]\n" },
    { "[1,2.5,-3e2]",
      "(array)\n (number) [1]\n (number) [2.5]\n (number) [-3e2]\n" },
    { "{\"a\":0x10}", "(object)\n (field)\n  (a) []\n  (number) [0x10]\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_literals_and_symbols
static int test_json_parse_literals_and_symbols (void)
{
  static const struct jp_ok_case cases[] = {
    { "[true,false]",
      "(array)\n (bool) [true]\n (bool) [false]\n" },
    { "[undefined]",  "(array)\n (reference) [undefined]\n" },
    { "[A,B]",        "(array)\n (symbol) [A]\n (symbol) [B]\n" },
    { "[$(ONE)]",     "(array)\n (symbol) [$(ONE)]\n" },
    { "[a.b-c]",      "(array)\n (symbol) [a.b-c]\n" },
    { "[x1,_y]",      "(array)\n (symbol) [x1]\n (symbol) [_y]\n" },
    /* look-alikes of keywords are symbols, not keywords */
    { "[truex,falsey,True,NULL]",
      "(array)\n (symbol) [truex]\n (symbol) [falsey]\n (symbol) [True]\n"
      " (symbol) [NULL]\n" },
    { "[Nan,infinity,Undefined]",
      "(array)\n (symbol) [Nan]\n (symbol) [infinity]\n (symbol) [Undefined]\n" },
    /* null and Infinity: node names are not documented; check success */
    { "[null]",       NULL },
    { "[Infinity]",   NULL },
    { "{\"a\":null}", NULL },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_structure_edge_cases
static int test_json_parse_structure_edge_cases (void)
{
  static const struct jp_ok_case cases[] = {
    /* duplicate keys are kept, in order */
    { "{\"a\":1,\"a\":2}",
      "(object)\n (field)\n  (a) []\n  (number) [1]\n"
      " (field)\n  (a) []\n  (number) [2]\n" },
    /* no whitespace at all */
    { "{\"a\":{\"b\":[1,2,{\"c\":true}]}}",
      "(object)\n"
      " (field)\n"
      "  (a) []\n"
      "  (object)\n"
      "   (field)\n"
      "    (b) []\n"
      "    (array)\n"
      "     (number) [1]\n"
      "     (number) [2]\n"
      "     (object)\n"
      "      (field)\n"
      "       (c) []\n"
      "       (bool) [true]\n" },
    /* mixed element types */
    { "[1,\"two\",true,[3],{\"k\":4}]",
      "(array)\n"
      " (number) [1]\n"
      " (string) [two]\n"
      " (bool) [true]\n"
      " (array)\n"
      "  (number) [3]\n"
      " (object)\n"
      "  (field)\n"
      "   (k) []\n"
      "   (number) [4]\n" },
    /* CRLF, tabs and blank lines everywhere */
    { "\r\n\t[\r\n\r\n\t1\t,\r\n 2\r\n]\r\n\t",
      "(array)\n (number) [1]\n (number) [2]\n" },
    { "{\r\n  \"a\" \r\n : \r\n 1 \r\n}",
      "(object)\n (field)\n  (a) []\n  (number) [1]\n" },
    /* value that is a multi-line string inside an object */
    { "{\"a\":\"x\ny\"}",
      "(object)\n (field)\n  (a) []\n  (string) [x\\ny]\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_fail_truncated
static int test_json_parse_fail_truncated (void)
{
  static const char *const cases[] = {
    "[", "{", "[[", "{{", "[{", "{[",
    "[1", "[1,", "[1,2", "[\"a\"", "[\"a\",",
    "{\"a\"", "{\"a\":", "{\"a\":1", "{\"a\":1,", "{\"a\":1,\"b\"",
    "{\"a\":[", "{\"a\":{", "{\"a\":[1,2", "{\"a\":{\"b\":1",
    /* unterminated strings (the scanner reports ERROR) */
    "\"", "\"abc", "[\"abc", "[\"abc]", "{\"a",
    "{\"a:1}", "{\"a\":\"b", "[\"a\nb]", "[\"\\\"]", "[\"abc\\",
  };
  return jp_run_fail_cases (cases, JSN_N (cases));
}

// name: test_json_parse_fail_mismatched_and_stray
static int test_json_parse_fail_mismatched_and_stray (void)
{
  /* Each of these fails inside the first value, or because the first
   * token cannot start a value. Input like "[]]" or "{}}" is NOT here:
   * the first value is complete, so it parses and the stray closer is
   * left in the reader for the next parse (see the stream tests). */
  static const char *const cases[] = {
    /* a token that cannot start a value */
    "]", "}", ":", ",", "]]", "}{", "][", "]x", "},", ":1",
    /* closer that does not match the opener */
    "[}", "{]", "[1}", "{\"a\":1]", "[{]}", "{\"a\":[}", "{\"a\":[1}",
    "{\"a\":{]}", "[}]", "{\"a\":[1,2}", "[[1,2}]",
    /* unclosed at end of input (also covered by the truncated tests) */
    "[[]", "[{}", "[[1],",
  };
  return jp_run_fail_cases (cases, JSN_N (cases));
}

#if 0
// Leaving this in for reference - the implementation allows comma misuse like
// this (leading, trailing commas, doubled-commas, etc).
// name: test_json_parse_fail_comma_and_colon_misuse
static int test_json_parse_fail_comma_and_colon_misuse (void)
{
  static const char *const cases[] = {
    /* leading and doubled commas (a trailing one is fine) */
    "[,]", "[,1]", "[1,,2]", "[1,,]", "{,}", "{,\"a\":1}",
    "{\"a\":1,,\"b\":2}", "{\"a\":1,,}",
    /* missing or misplaced colons and values */
    "{\"a\" 1}", "{\"a\"::1}", "{\"a\":}", "{\"a\":,}", "{:1}", "{:}",
    "{\"a\"}", "{\"a\",1}", "{\"a\":1:2}", "{\"a\":\"b\":\"c\"}",
    "[1:2]", "[:]", "[\"a\":1]",
  };
  return jp_run_fail_cases (cases, JSN_N (cases));
}
#endif

// name: test_json_parse_fail_bad_keys
static int test_json_parse_fail_bad_keys (void)
{
  /* Containers are never keys. (Whether a bare number may be a key is
   * not documented: see test_json_parse_probe.) */
  static const char *const cases[] = {
    "{[]:1}", "{{}:1}", "{[1]:1}", "{\"a\":1,[]:3}", "{\"a\":1,{}:3}",
  };
  return jp_run_fail_cases (cases, JSN_N (cases));
}

// name: test_json_parse_number_passthrough
static int test_json_parse_number_passthrough (void)
{
  /* Numbers are never validated or converted: any NUMBER token from the
   * scanner (see test_json_scan_numbers_malformed) becomes a (number)
   * node holding the raw text. */
#define JP_NUM1(x)  { "[" x "]", "(array)\n (number) [" x "]\n" }
  static const struct jp_ok_case cases[] = {
    JP_NUM1 ("1abc"), JP_NUM1 ("1e"), JP_NUM1 ("1E"), JP_NUM1 ("1e+"),
    JP_NUM1 ("1e-"), JP_NUM1 ("1.5e"), JP_NUM1 ("1.2.3"),
    JP_NUM1 ("12..13"), JP_NUM1 ("123eb"), JP_NUM1 ("2026-10-25"),
    JP_NUM1 ("1-2"), JP_NUM1 ("1+1"), JP_NUM1 ("1/2"),
    /* slashes stay inside a number, "//" included: no comment starts */
    JP_NUM1 ("1//2"), JP_NUM1 ("1//c"), JP_NUM1 ("-x//c"),
    /* bad octal, hex and binary */
    JP_NUM1 ("08"), JP_NUM1 ("09"), JP_NUM1 ("007e"), JP_NUM1 ("00.5"),
    JP_NUM1 ("0x"), JP_NUM1 ("0X"), JP_NUM1 ("0b"), JP_NUM1 ("0B"),
    JP_NUM1 ("0b2"), JP_NUM1 ("0b12"), JP_NUM1 ("0x1.8"), JP_NUM1 ("0xx"),
    JP_NUM1 ("-0x"),
    /* signs and dots with no digits */
    JP_NUM1 ("-"), JP_NUM1 ("+"), JP_NUM1 ("."), JP_NUM1 (".."),
    JP_NUM1 ("--1"), JP_NUM1 ("-+1"), JP_NUM1 ("+-1"), JP_NUM1 ("-a"),
    JP_NUM1 ("+a"), JP_NUM1 ("-x"), JP_NUM1 (".e5"), JP_NUM1 ("-."),
    JP_NUM1 ("+."),
    /* as object values, and several in a row */
    { "{\"a\":1abc}",
      "(object)\n (field)\n  (a) []\n  (number) [1abc]\n" },
    { "{\"a\":-}",
      "(object)\n (field)\n  (a) []\n  (number) [-]\n" },
    { "{\"a\":0x}",
      "(object)\n (field)\n  (a) []\n  (number) [0x]\n" },
    { "[1abc,0x,-,12..13]",
      "(array)\n (number) [1abc]\n (number) [0x]\n (number) [-]\n"
      " (number) [12..13]\n" },
  };
#undef JP_NUM1
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_fail_unrecognised_tokens
static int test_json_parse_fail_unrecognised_tokens (void)
{
  static const char *const cases[] = {
    "[|]", "[~]", "[`]", "[/]", "[/x]", "[a|b]", "[a/b]", "{/}",
    "[\x01]", "[\x7f]", "[\xff]", "[\xc3\xa9]", "[caf\xc3\xa9]",
    "[\xc2\xa0" "1]",                    /* NBSP is not whitespace */
    "{\"a\":|}", "{|:1}", "{\"a\"|1}",
  };
  return jp_run_fail_cases (cases, JSN_N (cases));
}

// name: test_json_parse_null_params
static int test_json_parse_null_params (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  xtree_node_t *node = NULL;

  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  node = xtree_json_parse (&err, NULL, 2000);
  if (node) {
    PERROR ("Parsing a NULL reader should return NULL\n");
    goto cleanup;
  }
  if (!err.libcode && !err.syscode) {
    PERROR ("Parsing a NULL reader should set the error object\n");
    goto cleanup;
  }
  if (!rdr_expect_wellformed_errobj (&err))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_json_parse_every_prefix_fails
static int test_json_parse_every_prefix_fails (void)
{
  /* The document is a single balanced container, so no proper prefix of
   * it can be valid. Every prefix must fail cleanly (no crash, no
   * success, error object set). Exercises every error path in the
   * parser at every token boundary and in the middle of every token. */
  static const char doc[] =
    "{\"a\":[1,2.5,\"s\\n\",true,{\"b\":null}],"
    "\"c\":{\"d\":-1e+3,},\"e\":[A,undefined,NaN,0x1F,],}";
  int rc = EXIT_SUCCESS;
  size_t len = strlen (doc);
  char *prefix = malloc (len + 1);

  if (!prefix) {
    PERROR ("Failed to allocate prefix buffer\n");
    return EXIT_FAILURE;
  }

  /* Sanity: the full document must parse. */
  if (!jp_expect_ok (doc, NULL))
    rc = EXIT_FAILURE;

  for (size_t n=1; n<len; n++) {
    memcpy (prefix, doc, n);
    prefix[n] = '\0';
    if (!jp_expect_fail (prefix)) {
      PERROR ("Prefix length %zu failed\n", n);
      rc = EXIT_FAILURE;
    }
  }

  free (prefix);
  return rc;
}

/* Build `depth` nested arrays: [[[...]]]. If `close` is false the closing
 * brackets are omitted. */
static char *jp_nested (size_t depth, bool close)
{
  char *buf = malloc (2 * depth + 1);
  if (!buf)
    return NULL;
  memset (buf, '[', depth);
  if (close)
    memset (buf + depth, ']', depth);
  buf[close ? 2 * depth : depth] = '\0';
  return buf;
}

// name: test_json_parse_deep_nesting
static int test_json_parse_deep_nesting (void)
{
  int rc = EXIT_SUCCESS;

  /* Moderate depth must work, and the result must have that depth. */
  const size_t depth = 100;
  char *good = jp_nested (depth, true);
  if (!good) {
    PERROR ("Failed to allocate input\n");
    return EXIT_FAILURE;
  }

  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;

  if (!jp_parse (good, &err, &reader, &node, 2000))
    rc = EXIT_FAILURE;
  else if (!node) {
    PERROR ("Depth %zu should parse\n", depth);
    rdr_print_errobj (&err);
    rc = EXIT_FAILURE;
  } else {
    size_t d = 1;
    xtree_node_t *cur = node;
    while (xtree_node_child_count (cur) == 1) {
      cur = xtree_node_child_get (&err, cur, 0);
      if (!cur)
        break;
      d++;
    }
    if (d != depth) {
      PERROR ("Expected nesting depth %zu, got %zu\n", depth, d);
      rc = EXIT_FAILURE;
    }
  }
  jp_cleanup (&err, &reader, &node);
  free (good);

  /* Unbalanced deep input must fail, not crash. */
  char *open = jp_nested (5000, false);
  if (!open) {
    PERROR ("Failed to allocate input\n");
    return EXIT_FAILURE;
  }
  if (!jp_expect_fail (open))
    rc = EXIT_FAILURE;
  free (open);

  /* Very deep input: the parser may accept it or reject it with a clean
   * error (e.g. a depth limit), but it must not crash or overflow the
   * stack. Drop this part if the parser is intentionally recursive and
   * unbounded. */
  char *deep = jp_nested (20000, true);
  if (!deep) {
    PERROR ("Failed to allocate input\n");
    return EXIT_FAILURE;
  }
  struct xtree_errobj_t err2 = { 0 };
  xtree_reader_t *reader2 = NULL;
  xtree_node_t *node2 = NULL;
  if (!jp_parse (deep, &err2, &reader2, &node2, 8000))
    rc = EXIT_FAILURE;
  else if (!node2 && !err2.libcode && !err2.syscode) {
    PERROR ("Depth 20000: NULL result without an error\n");
    rc = EXIT_FAILURE;
  }
  jp_cleanup (&err2, &reader2, &node2);
  free (deep);

  return rc;
}

// name: test_json_parse_large_inputs
static int test_json_parse_large_inputs (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;
  char *buf = NULL;
  char *want = NULL;
  const size_t count = 5000;

  /* 1. A large array: [0,1,2,...]. */
  buf = malloc (count * 8 + 3);
  if (!buf) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }
  size_t n = 0;
  buf[n++] = '[';
  for (size_t i=0; i<count; i++)
    n += (size_t)sprintf (buf + n, "%s%zu", i ? "," : "", i);
  buf[n++] = ']';
  buf[n] = '\0';

  if (!jp_parse (buf, &err, &reader, &node, 8000))
    goto cleanup;
  if (!node) {
    PERROR ("Large array failed to parse\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  if (xtree_node_child_count (node) != count) {
    PERROR ("Large array: expected %zu children, got %zu\n",
            count, xtree_node_child_count (node));
    goto cleanup;
  }
  xtree_node_t *last = xtree_node_child_get (&err, node, count - 1);
  const unsigned char *v = last ? xtree_node_value_get (&err, last) : NULL;
  char expect_last[32];
  snprintf (expect_last, sizeof expect_last, "%zu", count - 1);
  if (!v || ustrcmp (v, U (expect_last)) != 0) {
    PERROR ("Large array: wrong last value [%s]\n", v ? CS (v) : "(null)");
    goto cleanup;
  }
  jp_cleanup (&err, &reader, &node);
  free (buf);
  buf = NULL;

  /* 2. A large object: {"k0":0,"k1":1,...}: one field per key. */
  buf = malloc (count * 24 + 3);
  if (!buf) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }
  n = 0;
  buf[n++] = '{';
  for (size_t i=0; i<count; i++)
    n += (size_t)sprintf (buf + n, "%s\"k%zu\":%zu", i ? "," : "", i, i);
  buf[n++] = '}';
  buf[n] = '\0';

  if (!jp_parse (buf, &err, &reader, &node, 8000))
    goto cleanup;
  if (!node) {
    PERROR ("Large object failed to parse\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  if (xtree_node_child_count (node) != count) {
    PERROR ("Large object: expected %zu fields, got %zu\n",
            count, xtree_node_child_count (node));
    goto cleanup;
  }
  jp_cleanup (&err, &reader, &node);
  free (buf);
  buf = NULL;

  /* 3. One very long string value. */
  const size_t slen = 100000;
  buf = malloc (slen + 5);
  want = malloc (slen + 1);
  if (!buf || !want) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }
  for (size_t i=0; i<slen; i++)
    want[i] = (char)('a' + (i % 26));
  want[slen] = '\0';
  sprintf (buf, "[\"%s\"]", want);

  if (!jp_parse (buf, &err, &reader, &node, 2000))
    goto cleanup;
  if (!node) {
    PERROR ("Long string failed to parse\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  xtree_node_t *s = xtree_node_child_get (&err, node, 0);
  const unsigned char *sv = s ? xtree_node_value_get (&err, s) : NULL;
  if (!sv || ustrcmp (sv, U (want)) != 0) {
    PERROR ("Long string value was corrupted\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  jp_cleanup (&err, &reader, &node);
  free (buf);
  free (want);
  return ret;
}

// name: test_json_parse_from_file
static int test_json_parse_from_file (void)
{
  /* Same parser, driven by xtree_reader_file(): the document is longer
   * than any plausible reader buffer, so refills happen mid-token. */
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;
  char *fname = NULL;
  char *data = NULL;
  FILE *inf = NULL;
  const size_t count = 3000;
  size_t n = 0;

  data = malloc (count * 24 + 64);
  if (!data) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }
  n += (size_t)sprintf (data + n, "{\n  \"list\": [\n");
  for (size_t i=0; i<count; i++)
    n += (size_t)sprintf (data + n, "    \"item-%zu\",\n", i);
  n += (size_t)sprintf (data + n, "  ],\n  \"last\": true\n}\n");

  if (!rdr_write_tmpfile (data, n, &fname))
    goto cleanup;
  inf = fopen (fname, "rb");
  if (!inf) {
    PERROR ("Failed to reopen [%s]\n", fname);
    goto cleanup;
  }
  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }
  reader = xtree_reader_file (JP_NAME, inf);
  if (!reader) {
    PERROR ("Failed to create reader from file\n");
    goto cleanup;
  }
  node = xtree_json_parse (&err, reader, 8000);
  if (!node) {
    PERROR ("Failed to parse file\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  if (xtree_node_child_count (node) != 2) {
    PERROR ("Expected 2 fields in root object, got %zu\n",
            xtree_node_child_count (node));
    goto cleanup;
  }
  xtree_node_t *field = xtree_node_child_get (&err, node, 0);
  xtree_node_t *list = field ? xtree_node_child_get (&err, field, 1) : NULL;
  if (!list || xtree_node_child_count (list) != count) {
    PERROR ("Expected %zu list items, got %zu\n", count,
            list ? xtree_node_child_count (list) : (size_t)0);
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  jp_cleanup (&err, &reader, &node);
  if (inf)
    fclose (inf);
  if (fname) {
    remove (fname);
    free (fname);
  }
  free (data);
  return ret;
}

static int test_json_parse_missing_commas (void)
{
  /* Relaxed dialect: commas between array elements and object fields are
   * optional. The colon between a key and its value is not. */
  static const struct jp_ok_case cases[] = {
    { "[1 2]",
      "(array)\n (number) [1]\n (number) [2]\n" },
    { "[\"a\" \"b\"]",
      "(array)\n (string) [a]\n (string) [b]\n" },
    { "[\"a\"\"b\"]",
      "(array)\n (string) [a]\n (string) [b]\n" },
    { "[true false A]",
      "(array)\n (bool) [true]\n (bool) [false]\n (symbol) [A]\n" },
    { "[{} {}]",   "(array)\n (object)\n (object)\n" },
    { "[[] []]",   "(array)\n (array)\n (array)\n" },
    { "[{}{}[][]]",
      "(array)\n (object)\n (object)\n (array)\n (array)\n" },
    { "[1,2 3,4]",
      "(array)\n (number) [1]\n (number) [2]\n (number) [3]\n"
      " (number) [4]\n" },
    { "[1 2,]",    "(array)\n (number) [1]\n (number) [2]\n" },
    { "{\"a\":1 \"b\":2}",
      "(object)\n"
      " (field)\n  (a) []\n  (number) [1]\n"
      " (field)\n  (b) []\n  (number) [2]\n" },
    { "{\"a\":{} \"b\":[]}",
      "(object)\n"
      " (field)\n  (a) []\n  (object)\n"
      " (field)\n  (b) []\n  (array)\n" },
    { "{\"a\":\"x\"\"b\":\"y\"}",
      "(object)\n"
      " (field)\n  (a) []\n  (string) [x]\n"
      " (field)\n  (b) []\n  (string) [y]\n" },
    { "{\"a\":1,\"b\":2 \"c\":3,}",
      "(object)\n"
      " (field)\n  (a) []\n  (number) [1]\n"
      " (field)\n  (b) []\n  (number) [2]\n"
      " (field)\n  (c) []\n  (number) [3]\n" },
  };
  int rc = jp_run_ok_cases (cases, JSN_N (cases));

  /* The colon is still mandatory, and a key still needs a value. */
  static const char *const bad[] = {
    "{\"a\" \"b\"}", "{\"a\" 1}", "{\"a\":1 \"b\"}", "{\"a\":1 \"b\":}",
    "{\"a\":1 [2]}", "{\"a\":1 {}}",
  };
  if (jp_run_fail_cases (bad, JSN_N (bad)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  return rc;
}

/* Helpers for tests that parse several values from one reader. */

/* The next parse must return a tree whose node_dump_nv() equals exp. */
static bool jp_next_ok (xtree_reader_t *reader, const char *exp)
{
  bool ok = false;
  struct xtree_errobj_t err = { 0 };
  xtree_node_t *node = NULL;
  char *dump = NULL;

  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }
  node = xtree_json_parse (&err, reader, 2000);
  if (!node) {
    PERROR ("Expected a value, parse returned NULL\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  dump = node_dump_nv (&err, node);
  if (!dump || strcmp (dump, exp) != 0) {
    PERROR ("Mismatch\n--- expected ---\n%s--- actual ---\n%s",
            exp, dump ? dump : "(dump failed)\n");
    goto cleanup;
  }
  ok = true;

cleanup:
  free (dump);
  xtree_node_free (&node);
  xtree_errobj_reset (&err, 0);
  return ok;
}

/* The next parse must return NULL with NO error: nothing left to parse. */
static bool jp_next_none (xtree_reader_t *reader)
{
  bool ok = false;
  struct xtree_errobj_t err = { 0 };
  xtree_node_t *node = NULL;

  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }
  node = xtree_json_parse (&err, reader, 2000);
  if (node) {
    PERROR ("Expected no more values, but parse returned a tree\n");
    goto cleanup;
  }
  if (err.libcode || err.syscode) {
    PERROR ("End of input must not be an error\n");
    rdr_print_errobj (&err);
    goto cleanup;
  }
  ok = true;

cleanup:
  xtree_node_free (&node);
  xtree_errobj_reset (&err, 0);
  return ok;
}

/* The next parse must return NULL with a well-formed error set. */
static bool jp_next_fail (xtree_reader_t *reader)
{
  bool ok = false;
  struct xtree_errobj_t err = { 0 };
  xtree_node_t *node = NULL;

  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }
  node = xtree_json_parse (&err, reader, 2000);
  if (node) {
    PERROR ("Expected the parse to fail, but it returned a tree\n");
    goto cleanup;
  }
  if (!err.libcode && !err.syscode) {
    PERROR ("Parse returned NULL without setting the error object\n");
    goto cleanup;
  }
  ok = rdr_expect_wellformed_errobj (&err);

cleanup:
  xtree_node_free (&node);
  xtree_errobj_reset (&err, 0);
  return ok;
}

#define JP_CHECK(x) do { \
  if (!(x)) { PERROR ("Check failed: %s\n", #x); goto cleanup; } \
} while (0)

// name: test_json_parse_stream
static int test_json_parse_stream (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  char *buf = NULL;

  /* 1. Concatenated and line-separated values, then a clean end. */
  reader = xtree_reader_str (JP_NAME, U ("[1] [2]\n{\"a\":1}"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(array)\n (number) [1]\n"));
  JP_CHECK (xtree_reader_peek (reader) == ' ');   /* left right after ']' */
  JP_CHECK (jp_next_ok (reader, "(array)\n (number) [2]\n"));
  JP_CHECK (xtree_reader_peek (reader) == '\n');
  JP_CHECK (jp_next_ok (reader,
              "(object)\n (field)\n  (a) []\n  (number) [1]\n"));
  JP_CHECK (jp_next_none (reader));
  JP_CHECK (jp_next_none (reader));               /* and stays at the end */
  JP_CHECK (xtree_reader_eof (reader));
  xtree_reader_free (&reader);

  /* 2. No separator at all: nothing after a closer may be consumed. */
  reader = xtree_reader_str (JP_NAME, U ("[1][2]{}"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(array)\n (number) [1]\n"));
  JP_CHECK (xtree_reader_peek (reader) == '[');
  JP_CHECK (jp_next_ok (reader, "(array)\n (number) [2]\n"));
  JP_CHECK (xtree_reader_peek (reader) == '{');
  JP_CHECK (jp_next_ok (reader, "(object)\n"));
  JP_CHECK (jp_next_none (reader));
  xtree_reader_free (&reader);

  /* 3. A stray closer is left for the next parse, which then fails. */
  reader = xtree_reader_str (JP_NAME, U ("[]]"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(array)\n"));
  JP_CHECK (xtree_reader_peek (reader) == ']');
  JP_CHECK (jp_next_fail (reader));
  xtree_reader_free (&reader);

  reader = xtree_reader_str (JP_NAME, U ("{}}"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(object)\n"));
  JP_CHECK (xtree_reader_peek (reader) == '}');
  JP_CHECK (jp_next_fail (reader));
  xtree_reader_free (&reader);

  /* 4. Trailing scalars after a container are the next value. */
  reader = xtree_reader_str (JP_NAME, U ("{}x"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(object)\n"));
  JP_CHECK (xtree_reader_peek (reader) == 'x');
  JP_CHECK (jp_next_ok (reader, "(symbol) [x]\n"));
  JP_CHECK (jp_next_none (reader));
  xtree_reader_free (&reader);

  /* 5. Top-level scalars: the scanner reads one token and the reader is
   *    left at the first character after it ("12px" is NUMBER 12 then
   *    SYMBOL px; adjacent strings are two tokens). */
  reader = xtree_reader_str (JP_NAME, U ("12px"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(number) [12]\n"));
  JP_CHECK (xtree_reader_peek (reader) == 'p');
  JP_CHECK (jp_next_ok (reader, "(symbol) [px]\n"));
  JP_CHECK (jp_next_none (reader));
  xtree_reader_free (&reader);

  reader = xtree_reader_str (JP_NAME, U ("\"a\"\"b\" 42 43 true"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(string) [a]\n"));
  JP_CHECK (xtree_reader_peek (reader) == '"');
  JP_CHECK (jp_next_ok (reader, "(string) [b]\n"));
  JP_CHECK (jp_next_ok (reader, "(number) [42]\n"));
  JP_CHECK (jp_next_ok (reader, "(number) [43]\n"));
  JP_CHECK (jp_next_ok (reader, "(bool) [true]\n"));
  JP_CHECK (jp_next_none (reader));
  xtree_reader_free (&reader);

  /* 6. Whitespace-only input and trailing whitespace after the last value
   *    end cleanly, without an error. */
  reader = xtree_reader_str (JP_NAME, U ("[] \n\t \r\n"));
  JP_CHECK (reader);
  JP_CHECK (jp_next_ok (reader, "(array)\n"));
  JP_CHECK (jp_next_none (reader));
  xtree_reader_free (&reader);

  /* 7. Many one-per-line values (NDJSON style). */
  const size_t count = 2000;
  buf = malloc (count * 24 + 1);
  JP_CHECK (buf);
  size_t n = 0;
  for (size_t i=0; i<count; i++)
    n += (size_t)sprintf (buf + n, "{\"n\":%zu}\n", i);
  buf[n] = '\0';

  reader = xtree_reader_str (JP_NAME, (const unsigned char *)buf);
  JP_CHECK (reader);
  for (size_t i=0; i<count; i++) {
    char exp[64];
    snprintf (exp, sizeof exp,
              "(object)\n (field)\n  (n) []\n  (number) [%zu]\n", i);
    if (!jp_next_ok (reader, exp)) {
      PERROR ("Value %zu of the stream was wrong\n", i);
      goto cleanup;
    }
  }
  JP_CHECK (jp_next_none (reader));

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  free (buf);
  return ret;
}
#undef JP_CHECK
// name: test_json_parse_top_level_scalars
static int test_json_parse_top_level_scalars (void)
{
  static const struct jp_ok_case cases[] = {
    { "42",            "(number) [42]\n" },
    { "-1.5e+3",       "(number) [-1.5e+3]\n" },
    { "0x1F",          "(number) [0x1F]\n" },
    { "1abc",          "(number) [1abc]\n" },
    { "NaN",           "(number) [NaN]\n" },
    { "\"x\"",         "(string) [x]\n" },
    { "\"\"",          "(string) []\n" },
    { "\"a\nb\"",      "(string) [a\\nb]\n" },
    { "true",          "(bool) [true]\n" },
    { "false",         "(bool) [false]\n" },
    { "undefined",     "(reference) [undefined]\n" },
    { "A",             "(symbol) [A]\n" },
    { "$(ONE)",        "(symbol) [$(ONE)]\n" },
    { "truex",         "(symbol) [truex]\n" },
    { "null",          NULL },       /* node name not documented */
    { "Infinity",      NULL },
    /* surrounding whitespace is skipped */
    { " \t\r\n42 \n",  "(number) [42]\n" },
    { "\n\"x\"\n",     "(string) [x]\n" },
  };
  return jp_run_ok_cases (cases, JSN_N (cases));
}

// name: test_json_parse_maxdepth
static int test_json_parse_maxdepth (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader = NULL;
  xtree_node_t *node = NULL;
  char *input = NULL;

  const size_t maxdepth = 8;

  if (!xtree_errobj_reset (&err, 1024)) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  /* ---------------------------------------------------------------
   * Test 1: Nesting comfortably below maxdepth must succeed.
   */
  const size_t good_depth = 4;

  input = jp_nested (good_depth, true);
  if (!input) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }

  reader = xtree_reader_str (JP_NAME, (const unsigned char *)input);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  node = xtree_json_parse (&err, reader, maxdepth);
  if (!node) {
    PERROR ("Depth %zu should parse with maxdepth %zu\n",
            good_depth, maxdepth);
    rdr_print_errobj (&err);
    goto cleanup;
  }

  /* Verify that the resulting tree has the expected depth. */
  size_t depth = 1;
  xtree_node_t *cur = node;

  while (xtree_node_child_count (cur) == 1) {
    cur = xtree_node_child_get (&err, cur, 0);
    if (!cur) {
      PERROR ("Failed to retrieve child at depth %zu\n", depth);
      goto cleanup;
    }
    depth++;
  }

  if (depth != good_depth) {
    PERROR ("Expected depth %zu, got %zu\n", good_depth, depth);
    goto cleanup;
  }

  free (input);
  input = NULL;
  xtree_node_free (&node);
  xtree_reader_free (&reader);

  /* ---------------------------------------------------------------
   * Test 2: Nesting beyond maxdepth + 2 must fail.
   */
  const size_t bad_depth = maxdepth + 3;

  input = jp_nested (bad_depth, true);
  if (!input) {
    PERROR ("Failed to allocate input\n");
    goto cleanup;
  }

  reader = xtree_reader_str (JP_NAME, (const unsigned char *)input);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);

  node = xtree_json_parse (&err, reader, maxdepth);
  if (node) {
    PERROR ("Depth %zu should exceed maxdepth %zu\n",
            bad_depth, maxdepth);
    goto cleanup;
  }

  if (!err.libcode && !err.syscode) {
    PERROR ("Depth limit failure did not set the error object\n");
    goto cleanup;
  }

  if (!rdr_expect_wellformed_errobj (&err))
    goto cleanup;

  xtree_errobj_reset (&err, 1024);
  ret = EXIT_SUCCESS;

cleanup:
  free (input);
  xtree_node_free (&node);
  xtree_reader_free (&reader);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

/* ------------------------------------------------------------------
 * Roundtrip test for xtree_json_parse_dump()
 *
 * Verifies that the dump function produces output that:
 *   1. Matches a known-good expected string, and
 *   2. Is idempotent — parsing the dump and dumping again yields the
 *      exact same string.
 *
 * The source JSON exercises every value type the parser supports:
 * bool (true/false), string, number (decimal int, float, hex), NaN,
 * undefined (reference), symbol, null, Infinity, nested array and a
 * nested object.
 *
 * The expected string assumes compact output at indent=0 (no spaces
 * after ':' or ','). If the actual dump format differs, the failure
 * message prints the real output so this literal can be corrected —
 * the roundtrip (output2 == output1) check is independent of the exact
 * format and will still catch any non-determinism or re-parse failure.
 * ------------------------------------------------------------------ */

// name: test_json_parse_dump_roundtrip
static int test_json_parse_dump_roundtrip (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  xtree_reader_t *reader1 = NULL;
  xtree_reader_t *reader2 = NULL;
  xtree_node_t *node1 = NULL;
  xtree_node_t *node2 = NULL;
  char *output1 = NULL;
  char *output2 = NULL;

  /* Source JSON: one of every supported value type. The formatting
   * (whitespace, alignment) is deliberately not compact so that the
   * dump must canonicalise it. */
  static const unsigned char *source = (const unsigned char *)
    "{\n"
    "  \"bool_true\":    true,\n"
    "  \"bool_false\":   false,\n"
    "  \"string_val\":   \"hello world\",\n"
    "  \"number_int\":   42,\n"
    "  \"number_float\": -3.14e+5,\n"
    "  \"number_hex\":   0x1F,\n"
    "  \"nan_val\":      NaN,\n"
    "  \"undefined_val\": undefined,\n"
    "  \"symbol_val\":   mySymbol,\n"
    "  \"null_val\":     null,\n"
    "  \"infinity_val\": Infinity,\n"
    "  \"nested_array\": [1, \"two\", true, null, NaN],\n"
    "  \"nested_object\": {\n"
    "    \"inner_str\":   \"inner\",\n"
    "    \"inner_num\":   7,\n"
    "    \"inner_bool\":  false\n"
    "  }\n"
    "}";

  /* Expected dump at indent=3. Built by hand from the source above;
   * if the library's compact format differs (e.g. it adds a space
   * after ':'), copy the actual output from the failure message. */
  static const char *expected =
"{\n"
"   \"bool_true\":  true,\n"
"   \"bool_false\":  false,\n"
"   \"string_val\":  \"hello world\",\n"
"   \"number_int\":  42,\n"
"   \"number_float\":  -3.14e+5,\n"
"   \"number_hex\":  0x1F,\n"
"   \"nan_val\":  NaN,\n"
"   \"undefined_val\":  undefined,\n"
"   \"symbol_val\":  mySymbol,\n"
"   \"null_val\":  null,\n"
"   \"infinity_val\":  Infinity,\n"
"   \"nested_array\": [ \n"
"       1,\n"
"       \"two\",\n"
"       true,\n"
"       null,\n"
"       NaN\n"
"   ],\n"
"   \"nested_object\": {\n"
"      \"inner_str\":  \"inner\",\n"
"      \"inner_num\":  7,\n"
"      \"inner_bool\":  false\n"
"   }\n"
"}";

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  /* 1. Parse the source JSON into node1. */
  reader1 = xtree_reader_str ("dump-source", source);
  if (!reader1) {
    PERROR ("Failed to create reader for source JSON\n");
    goto cleanup;
  }
  node1 = xtree_json_parse (&err, reader1, 2000);
  if (!node1) {
    PERROR ("Failed to parse source JSON\n");
    goto cleanup;
  }

  /* 2. Dump node1 to output1. */
  output1 = xtree_json_parse_dump (&err, node1, 3);
  if (!output1) {
    PERROR ("Failed to dump node1\n");
    goto cleanup;
  }

  /* 3. Parse output1 back into a new node2. */
  reader2 = xtree_reader_str ("dump-round2", (const unsigned char *)output1);
  if (!reader2) {
    PERROR ("Failed to create reader for round 2\n");
    goto cleanup;
  }
  node2 = xtree_json_parse (&err, reader2, 2000);
  if (!node2) {
    PERROR ("Failed to re-parse the dumped output\n");
    goto cleanup;
  }

  /* 4. Dump node2 to output2. */
  output2 = xtree_json_parse_dump (&err, node2, 3);
  if (!output2) {
    PERROR ("Failed to dump node2\n");
    goto cleanup;
  }

  /* 5. output1 must match the expected string. */
  if (strcmp (output1, expected) != 0) {
    PERROR ("output1 does not match expected dump\n"
            "--- expected ---\n[%s]\n"
            "--- output1 ---\n[%s]\n",
            expected, output1);
    goto cleanup;
  }

  /* 6. output2 must also match the expected string (idempotency). */
  if (strcmp (output2, expected) != 0) {
    PERROR ("output2 does not match expected dump\n"
            "--- expected ---\n[%s]\n"
            "--- output2 ---\n[%s]\n",
            expected, output2);
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  free (output2);
  free (output1);
  xtree_node_free (&node2);
  xtree_reader_free (&reader2);
  xtree_node_free (&node1);
  xtree_reader_free (&reader1);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
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

    /* Tests for xtree_json_scan() */
    TEST (test_json_scan_eof_empty),
    TEST (test_json_scan_eof_whitespace_only),
    TEST (test_json_scan_punctuation_each),
    TEST (test_json_scan_punctuation_sequence),
    TEST (test_json_scan_punctuation_whitespace_separated),
    TEST (test_json_scan_grammar_agnostic),
    TEST (test_json_scan_literals_each),
    TEST (test_json_scan_literals_sequence),
    TEST (test_json_scan_keyword_vs_lookalike),
    TEST (test_json_scan_literals_in_structure),
    TEST (test_json_scan_strings_valid),
    TEST (test_json_scan_strings_sequence),
    TEST (test_json_scan_numbers_valid),
    TEST (test_json_scan_numbers_malformed),
    TEST (test_json_scan_numbers_delimited),
    TEST (test_json_scan_whitespace_kinds),
    TEST (test_json_scan_full_document),
    TEST (test_json_scan_token_filename),
    TEST (test_json_scan_string_escapes_not_decoded),
    TEST (test_json_scan_strings_multiline),
    TEST (test_json_scan_symbols_single),
    TEST (test_json_scan_symbols_delimited),
    TEST (test_json_scan_unrecognised_single),
    TEST (test_json_scan_unrecognised_delimited),
    TEST (test_json_scan_special_values),
    TEST (test_json_scan_comments_single),
    TEST (test_json_scan_comments_in_context),
    TEST (test_json_scan_error_unterminated_string),
    TEST (test_json_scan_error_after_tokens),
    TEST (test_json_scan_null_reader),
    TEST (test_json_scan_tokens_independent),
    TEST (test_json_scan_long_string),
    TEST (test_json_scan_long_number),
    TEST (test_json_scan_many_tokens),
    TEST (test_json_scan_long_symbol_and_comment),

    /* Tests for xtree_scan_token_dump() */
    TEST (test_scan_token_dump),

    /* Tests for xtree_json_parse() */
    TEST (test_json_parse_happy),

    TEST (test_json_parse_empty_containers),
    TEST (test_json_parse_trailing_commas),
    TEST (test_json_parse_string_edge_cases),
    TEST (test_json_parse_number_forms),
    TEST (test_json_parse_literals_and_symbols),
    TEST (test_json_parse_structure_edge_cases),
    TEST (test_json_parse_fail_truncated),
    TEST (test_json_parse_fail_mismatched_and_stray),
    // TEST (test_json_parse_fail_comma_and_colon_misuse),
    TEST (test_json_parse_fail_bad_keys),
    TEST (test_json_parse_number_passthrough),
    TEST (test_json_parse_fail_unrecognised_tokens),
    TEST (test_json_parse_null_params),
    TEST (test_json_parse_every_prefix_fails),
    TEST (test_json_parse_deep_nesting),
    TEST (test_json_parse_large_inputs),
    TEST (test_json_parse_from_file),
    TEST (test_json_parse_missing_commas),
    TEST (test_json_parse_top_level_scalars),
    TEST (test_json_parse_stream),
    TEST (test_json_parse_maxdepth),
    TEST (test_json_parse_dump_roundtrip),
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

  printf ("testing tree library\n"
          "Diff this output against ./tests/outputs/tree_test.expected\n");
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
