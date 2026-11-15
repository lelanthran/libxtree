
#ifndef H_XTREE_GTML_SCAN
#define H_XTREE_GTML_SCAN

/* ************************************************************************
 * General Text Markup Language (GTML) — a relaxed form of HTML.
 *
 * This file specifies the grammar only: input is scanned into tokens and how
 * those tokens are parsed into a tree. This is the scanner spec only.
 * Processing semantics (script/style inclusion, entity expansion, etc.) are
 * defined downstream of this spec and are out of scope here.
 *
 *
 * =====================================================================
 * 1. INTRODUCTION — GENERAL FORM
 * =====================================================================
 *
 * A GTML document is a sequence of nodes: elements, text, comments,
 * CDATA sections, and directives. Multiple top-level elements are
 * permitted; the result is a forest, not necessarily a single root.
 *
 * Example:
 *     <tag name="attribute"> Some text </tag>
 *
 * Tags:
 *     <name>                  open
 *     </name>                 close
 *     <name/>                 self-closing
 *     <name attr>             attribute with empty value
 *     <name attr=>            attribute with empty value
 *     <name attr="">          attribute with empty value
 *     <name attr=value>       unquoted identifier
 *     <name attr=1234>        unquoted number
 *     <name attr="value">     double-quoted value
 *     <name attr='value'>     single-quoted value
 *
 * Nesting is strict: every non-self-closing tag must be explicitly
 * closed. Stray or mismatched close tags are errors.
 *
 * Encoding is UTF-8 with no BOM. Input outside this spec is an error.
 *
 *
 * =====================================================================
 * 2. SCANNER RULES
 * =====================================================================
 *
 * The scanner consumes input and emits a token stream. The parser
 * consumes the token stream (section 3). A well-formed input yields
 * a well-formed token stream; the scanner does not know about grammar
 * positions.
 *
 * The biggest problem that SGML-like languages are not easily split into
 * tokens. For example, in:
 *    <tag a=b> c=d </tag>
 * A scanner needs context to retrieve `a` as a case-insensitive ID (using ID
 * rules) vs `c` as a case-sensitive string. Same problem for `b` vs `d`.
 *
 * To get around this, the token returned by the scanner are much simpler, and
 * leaves the correct determination of the token to the parser, which has
 * context.
 *
 * The tokens that can be returned:
 *    <               LT: self-explanatory
 *    >               GT: self-explanatory
 *    =               EQUAL: self-explanatory
 *    /               SLASH: self-explanatory
 *    !               EXCL: self-explanatory
 *    #               HASH: Note that parser will ensure preceding `<`
 *    ?               QMARK: Note that parser will ensure preceding `<`
 *    [               LBRACKET: self-explanatory
 *    ]               RBRACKET: self-explanatory
 *    whitespace      WSPACE: Parser will coalesce runs of whitespace
 *    "..."           STRING: quotes saved in the token text
 *    '...'           Ditto
 *    &...;           ECODE: character code token, text contains the code and
 *                    the leading and trailing delimiters, `&` and `;`. The
 *                    character after the `&` can only be `#` or an alphabet
 *                    character.
 tokens.
 *    <![CDATA[...]]> A single CDATA token containing everything verbatim.
 *    ...             SYMBOL: Any run of characters not delimited by above
 *
 * The parser will then have to decide, depending on context, whether a SYMBOL
 * is verbatim innertext or an ID (tagname, attribute-name or attribute-value).
 *
 * This is a rather generous interpretation resulting in tagnames like `<12>`
 * and attribute names like `&lt;exploit&gt`.
 *
 *
 * --- Forbidden ---
 *
 * Any input that cannot be scanned using the (very generous) rules above will
 * result in an error; for example missing string terminator, missing `;` after
 * an escape code, unterminated `<`, etc.
 *
 */

enum xtree_gtml_token_id_t {
  xtree_gtml_token_SENTINEL     = XTREE_SCAN_TOKEN_SENTINEL,
  xtree_gtml_token_UNRECOGNISED = XTREE_SCAN_TOKEN_UNRECOGNISED,
  xtree_gtml_token_ERROR        = XTREE_SCAN_TOKEN_ERROR,
  xtree_gtml_token_EOF          = XTREE_SCAN_TOKEN_EOF,
  xtree_gtml_token_LT,              // >
  xtree_gtml_token_GT,              // <
  xtree_gtml_token_EQUAL,           // =
  xtree_gtml_token_SLASH,           // /
  xtree_gtml_token_EXCL,            // !
  xtree_gtml_token_HASH,            // #
  xtree_gtml_token_QMARK,           // ?
  xtree_gtml_token_LBRACKET,        // [
  xtree_gtml_token_RBRACKET,        // ]
  xtree_gtml_token_WSPACE,          // Multi-run whitespace
  xtree_gtml_token_STRING,          // Quoted string (with quotes)
  xtree_gtml_token_ECODE,           // Escaped code - &...;
  xtree_gtml_token_CDATA,           // CDATA
  xtree_gtml_token_SYMBOL,          // Everything else
};


#ifdef __cplusplus
extern "C" {
#endif

  // Returns the next TML token found in the supplied reader_t object.
  // Whitespace between tokens is skipped. At the end of the input an
  // xtree_gtml_token_EOF token is returned (repeatedly, if called again).
  // On an unterminated string an xtree_gtml_token_ERROR token is returned.
  xtree_scan_token_t *xtree_gtml_scan (struct xtree_errobj_t *err,
                                       xtree_reader_t *reader);

  // Get a string representation of the token id.
  const char *xtree_gtml_token_id_str (enum xtree_gtml_token_id_t id);

#ifdef __cplusplus
};
#endif


#endif


