
#ifndef H_XTREE_JSON_SCAN
#define H_XTREE_JSON_SCAN

  // Token ids returned by xtree_scan_token_id() for tokens produced by
  // xtree_json_scan().
  //
  // The scanner is deliberately more lenient than strict JSON. The text of
  // a token is always the raw source text, except where noted:
  //
  //   - Strings include their surrounding double quotes. Escape sequences
  //     are never decoded or validated; whatever follows a backslash is
  //     kept as-is (a backslash still protects the following character, so
  //     \" does not end the string). Strings may span lines: a literal LF
  //     in the input appears as the two characters \n in the text, and a
  //     literal CR as \r (so CRLF becomes \r\n). Nothing else is changed.
  //   - Numbers: an optional sign (+ or -) followed by decimal digits with
  //     an optional fraction and exponent (".5", "5.", "1e+3" are valid), or
  //     0x/0X + hex digits, 0b/0B + binary digits, or 0 + octal digits.
  //   - Comments run from "//" to the end of the line (the LF, and a CR
  //     directly before it, are not part of the text). A comment may start
  //     anywhere outside a string, even directly after another token.
  //   - Anything else that is not whitespace, punctuation, a string or a
  //     comment is a "word": a run of characters ending at whitespace, one
  //     of { } [ ] : , a double quote, or "//". The whole word is then
  //     classified: a valid number is NUMBER; exactly true, false, null,
  //     Infinity, +Infinity, -Infinity, NaN or undefined is the matching
  //     token; anything else (1abc, truex, $(ONE), @, ...) is a SYMBOL.
  //
  // Whitespace is space, tab, CR and LF; it is skipped between tokens. The
  // EOF and ERROR tokens have an empty string as their text.

  enum xtree_json_token_id_t {
    xtree_json_token_SENTINEL   = -2, // Reserved for internal use
    xtree_json_token_ERROR      = -1, // Error in scanner
    xtree_json_token_EOF        = 0,  // End of input
    xtree_json_token_LBRACE,          // {
    xtree_json_token_RBRACE,          // }
    xtree_json_token_LBRACKET,        // [
    xtree_json_token_RBRACKET,        // ]
    xtree_json_token_COLON,           // :
    xtree_json_token_COMMA,           // ,
    xtree_json_token_STRING,          // "..." (quotes removed, escapes as-is)
    xtree_json_token_NUMBER,          // -12.5e+3, .5, +1, 0x1F, 017, 0b101
    xtree_json_token_TRUE,            // true
    xtree_json_token_FALSE,           // false
    xtree_json_token_NULL,            // null
    xtree_json_token_COMMENT,         // // to end of line
    xtree_json_token_SYMBOL,          // any other unquoted word
    xtree_json_token_INFINITY,        // Infinity, +Infinity
    xtree_json_token_NEG_INFINITY,    // -Infinity
    xtree_json_token_NAN,             // NaN, +NaN
    xtree_json_token_NEG_NAN,         // -NaN
    xtree_json_token_UNDEFINED,       // undefined
    xtree_json_token_UNRECOGNISED,    // Unknown tokens
  };


#ifdef __cplusplus
extern "C" {
#endif

  // Returns the next JSON token found in the supplied reader_t object.
  // Whitespace between tokens is skipped. At the end of the input an
  // xtree_json_token_EOF token is returned (repeatedly, if called again).
  // On an unterminated string an xtree_json_token_ERROR token is returned.
  xtree_scan_token_t *xtree_json_scan (xtree_reader_t *reader);

  // Get a string representation of the token id.
  const char *xtree_json_token_id_str (enum xtree_json_token_id_t id);

#ifdef __cplusplus
};
#endif


#endif


