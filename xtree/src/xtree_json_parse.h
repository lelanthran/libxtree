
#ifndef H_XTREE_JSON_PARSE
#define H_XTREE_JSON_PARSE

/*
 * xtree_json_parse(): parse one JSON-like value from a reader into an xtree.
 *
 * The dialect is a lenient superset of JSON, built on the tokens produced
 * by xtree_json_scan() (see xtree_json_scan.h for the lexical rules). The
 * parser's job is structure only. Values are never converted, validated or
 * decoded: the tree is an interchange format, so what a string, number or
 * symbol means is up to the consumer.
 *
 * Tree shape (nodes are named as shown; atoms keep their text as the value):
 *
 *   {...}        LIST  "object". Each member is a LIST "field" with exactly
 *                two children: an ATOM named after the key (empty value),
 *                then the value node. Duplicate keys are kept, in order.
 *   [...]        LIST  "array". One child per element, in order.
 *   "text"       ATOM  "string",    value is the text without the quotes.
 *   12, 0x1F     ATOM  "number",    value is the raw text.
 *   NaN          ATOM  "number",    value "NaN".
 *   true, false  ATOM  "bool",      value "true" or "false".
 *   undefined    ATOM  "reference", value "undefined".
 *   word         ATOM  "symbol",    any other unquoted word (a, $(ONE), x-1).
 *   null         TODO: node name.
 *   Infinity     TODO: node name.
 *
 * Strings:
 *   - Escape sequences are never decoded or validated: the value is the
 *     text as written, minus the enclosing quotes ("a\nb" is the four
 *     characters a \ n b, and "\q" is accepted).
 *   - Strings may span lines. A literal LF in the input is stored as the
 *     two characters \n, and a literal CR as \r (CRLF becomes \r\n).
 *   - Bytes >= 0x80 are passed through untouched, so UTF-8 survives.
 *   - Object keys must be strings. The key is the node name of the first
 *     child of the field.
 *
 * Numbers:
 *   - A number is stored verbatim as text. Hex (0x1F), binary (0b101),
 *     octal (017), ".5", "5.", "+1" and "1e+3" are all accepted.
 *   - Malformed numbers are accepted too and stored as written: 1abc,
 *     12..13, 0x, 08, -, 2026-10-25. The parser never rejects a number.
 *
 * Structure:
 *   - Trailing commas are allowed in objects and arrays ([1,2,] and
 *     {"a":1,}).
 *   - Leading or doubled commas ([,1], [1,,2], {,}) are errors.
 *   - A key must be followed by a colon and a value. A missing colon,
 *     missing value, or colon in an array is an error.
 *   - A closing bracket that does not match its opener is an error.
 *   - Input that ends in the middle of a value (including an unterminated
 *     string) is an error. An unrecognised character ("|", "~", a lone "/",
 *     bytes >= 0x80 outside a string) is an error.
 *   - Nesting depth is limited only by the parser's stack; very deep input
 *     either parses or fails with an error, and never crashes.
 *   - Comments (//) are scanned but not supported by the parser yet; they
 *     currently cause an error. TODO: planned for a future release.
 *
 * Streams and return values:
 *   - Exactly one value is parsed per call, and the reader is left
 *     positioned directly after it. Anything following (including a second
 *     value, or a stray closing bracket) is left unread for the next call.
 *     This allows concatenated documents and one-value-per-line input.
 *   - Success returns the root node.
 *   - End of input before any value (empty or whitespace-only input) is not
 *     an error: NULL is returned and `err` is left clear. Callers can tell
 *     this from a failure, where NULL is returned and `err` is set.
 *   - After a failed parse the reader's position is unspecified; do not try
 *     to resynchronise.
 *   - TODO: whether top-level scalars ("42", "x") and missing commas
 *     between elements ([1 2]) are accepted. Existing tests show a comma is
 *     not required after a closing } or ].
 */

#ifdef __cplusplus
extern "C" {
#endif

  xtree_node_t *xtree_json_parse (struct xtree_errobj_t *err,
                                  xtree_reader_t *reader,
                                  size_t max_depth);

  // Convert a JSON that was parsed into an xtree_node_t back into a JSON
  // string. The caller is responsible for freeing the returned value.
  char *xtree_json_parse_dump (struct xtree_errobj_t *err,
                               const xtree_node_t *json_node,
                               size_t indent);


#ifdef __cplusplus
};
#endif


#endif


