
#ifndef H_XTREE_GTML_SCAN
#define H_XTREE_GTML_SCAN

/* ************************************************************************
 * General Text Markup Language (GTML) — a relaxed form of HTML.
 *
 * This file specifies the grammar only: how input is scanned into tokens
 * and how those tokens are assembled into a tree. Processing semantics
 * (script/style inclusion, entity expansion, etc.) are defined downstream
 * of this spec and are out of scope here.
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
 * --- Whitespace ---
 *
 * Whitespace is any of: space, \t, \n, \v, \f, \r  (i.e. the set
 * matched by isspace() in the C locale).
 *
 * --- Identifiers ---
 *
 * Tag names, attribute names, and unquoted attribute values, other than
 * unquoted numbers, are identifiers.
 *
 *     start:  [A-Za-z]
 *     body:   [A-Za-z0-9_:.+-]
 *
 * Scanning an identifier ends at the first character outside the body
 * set, including whitespace, '=', '/', '>', '&', '<', quotes, etc.
 * The terminating character is not consumed by the identifier scan
 * (the scanner will attempt to produce the next token from it).
 *
 * > NOTE: attribute values can be either quoted strings, unquoted
 * >       identifiers or numbers.
 *
 * All identifiers are UPPER-CASED on storage — including identifiers
 * appearing as unquoted attribute values. To preserve case in an
 * attribute value, the author must quote it.
 *
 * > NOTE: The CDATA identifier, explained in below, is only recognised
 * >       when uppercased in the input.
 *
 * --- Escapes / entities ---
 *
 * The scanner recognizes three escape shapes:
 *
 *     &<identifier>;  identifier is one of a fixed named set (specified
 *                     separately)
 *     &#DDDD;         one or more decimal digits, unlimited
 *     &#xHHHH;        one or more hex digits, unlimited
 *
 * On match, the scanner emits { ESCAPED_TEXT, <payload> } where the
 * payload is the name or the numeric string.
 *
 * > NOTE: Even though unlimited digits may appear in the escape code, the
 * >       processor may have limits on number size or bit-width magnitude.
 *
 * If '&' is not followed by a well-formed escape shape, the scanner
 * emits { ESCAPED_TEXT, "" }.
 *
 * --- Comments ---
 *
 *     opener:     <#
 *     terminator: #>
 *
 * Comment body is any sequence of characters not containing the
 * terminator "#>". Comments do not nest.
 *
 * The scanner emits a { COMMENT, "..." } token.
 *
 * --- CDATA ---
 *
 *     opener:     <![CDATA[
 *     terminator: ]]>
 *
 * CDATA content is literal character data: no tag scanning, no entity
 * scanning, no comment scanning. The terminator may not appear inside
 * the content. CDATA does not nest.
 *
 * CDATA is allowed only in text content positions (element children
 * and document level). It is not allowed inside tags (attribute
 * position) and not inside comments.
 *
 * CDATA content is preserved verbatim — whitespace is not coalesced
 * inside CDATA.
 *
 * --- Directives ---
 *
 *     <? ... ?>
 *
 * A directive is literal text ending at the first "?>". Content
 * between is not scanned for tags, entities, comments, or CDATA. A
 * directive is allowed everywhere a tag is allowed.
 *
 * Exact directives and their semantics are specified separately.
 *
 *
 * --- DOCTYPE (ignored) ---
 *
 * The literal sequences <!doctype gtml> and <!doctype gtml/> appearing
 * at the top level are ignored by the scanner (produce no tokens).
 * No other DOCTYPE form is recognized.
 * If present, the `<!doctype>` node must be specified on the first
 * line of the input, appearing in full on that line.
 *
 * --- Forbidden ---
 *
 * A bare '<' that does not begin one of the above constructs is a
 * scanner error.
 *
 *
 * =====================================================================
 * 3. PARSER RULES
 * =====================================================================
 *
 * The parser consumes the token stream and produces a tree. Errors
 * in this section are parse errors; they are distinct from scanner
 * errors but both are fatal to the parse.
 *
 * --- Identifiers and attribute values ---
 *
 *   - No whitespace is allowed around '='. A whitespace between an
 *     identifier and '=' terminates the identifier; the '=' then
 *     begins the next token and is a parse error in that position.
 *
 *   - Unquoted attribute values end at whitespace or '>'. Because
 *     an unquoted value is an identifier token, '=' is outside the
 *     identifier body and therefore ends it; a subsequent '=' is
 *     a parse error at the attribute-position grammar rule.
 *
 *   - Quoted values ('...' or "...") preserve all characters
 *     verbatim, including whitespace, newlines, and case. Entities
 *     inside quoted values are scanned as ESCAPED_TEXT tokens.
 *
 *   - Duplicate attribute names are permitted and stored in the
 *     order encountered.
 *
 * --- Whitespace coalescing ---
 *
 *   Runs of whitespace are coalesced into a single space in text
 *   content, in attribute positions, and at document level. Whitespace
 *   inside CDATA and inside quoted attribute values is preserved
 *   verbatim and is never coalesced.
 *
 *   Whitespace at the document level (i.e. between nodes at that level)
  *  is ignored.
 *
 * --- Structure ---
 *
 *   - Multiple top-level elements are permitted.
 *   - Every non-self-closing tag must be closed before its parent
 *     is closed, and before EOF.
 *   - A close tag must match the innermost open tag (case-insensitive,
 *     since identifiers are upper-cased on storage).
 *   - Stray or mismatched close tags are parse errors.
 *   - A DOCTYPE-ignore sequence (<!doctype gtml> or <!doctype gtml/>)
 *     is permitted only at document level, and produces no node.
 *
 * --- Comments and directives in the grammar ---
 *
 *   - Comments are permitted as element children and at document
 *     level. They are forbidden inside a tag (attribute position).
 *
 *   - Directives are permitted wherever normal tags are permitted.
 *     They are ignored inside CDATA and inside comments (those are
       literal contexts).
 *
 * --- Errors ---
 *
 *   Every token that is syntactically valid but appears in a grammar
 *   position where it is not permitted is a parse error. Parse errors
 *   and scanner errors are both fatal to the parse; no recovery is
 *   attempted.
 *
 *
 * =====================================================================
 * 4. TREE CONSTRUCTION
 * =====================================================================
 * TBD. For now, it is sufficient to state that all tags, including
 * directives, CDATA, comments, etc are stored as nodes in the resulting
 * tree.
 */

  enum xtree_gtml_token_id_t {
    xtree_gtml_token_RESERVED   = -2, // Reserved for internal use
    xtree_gtml_token_ERROR      = -1, // Error in scanner
    xtree_gtml_token_EOF        = 0,  // End of input
    xtree_gtml_token_LBRACE,          // {
    xtree_gtml_token_RBRACE,          // }
    xtree_gtml_token_LBRACKET,        // [
    xtree_gtml_token_RBRACKET,        // ]
    xtree_gtml_token_COLON,           // :
    xtree_gtml_token_COMMA,           // ,
    xtree_gtml_token_STRING,          // "..." (quotes removed, escapes as-is)
    xtree_gtml_token_NUMBER,          // -12.5e+3, .5, +1, 0x1F, 017, 0b101
    xtree_gtml_token_TRUE,            // true
    xtree_gtml_token_FALSE,           // false
    xtree_gtml_token_NULL,            // null
    xtree_gtml_token_COMMENT,         // // to end of line
    xtree_gtml_token_SYMBOL,          // any other unquoted word
    xtree_gtml_token_INFINITY,        // Infinity, +Infinity
    xtree_gtml_token_NEG_INFINITY,    // -Infinity
    xtree_gtml_token_NAN,             // NaN, +NaN
    xtree_gtml_token_NEG_NAN,         // -NaN
    xtree_gtml_token_UNDEFINED,       // undefined
    xtree_gtml_token_UNRECOGNISED,    // Unknown tokens
  };


#ifdef __cplusplus
extern "C" {
#endif

  // Returns the next TML token found in the supplied reader_t object.
  // Whitespace between tokens is skipped. At the end of the input an
  // xtree_gtml_token_EOF token is returned (repeatedly, if called again).
  // On an unterminated string an xtree_gtml_token_ERROR token is returned.
  xtree_scan_token_t *xtree_gtml_scan (xtree_reader_t *reader);

  // Get a string representation of the token id.
  const char *xtree_gtml_token_id_str (enum xtree_gtml_token_id_t id);

#ifdef __cplusplus
};
#endif


#endif


