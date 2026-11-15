
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
 * --Token scanning rules--
 *
 * Tokens are scanned using the following rules. Unless otherwise stated,
 * delimiters are excluded from the token text.
 *
 * WSPACE:
 *    One or more characters from " \r\n\t\f".
 *    Consecutive whitespace characters are returned as a single token.
 *
 * CDATA:
 *    <![CDATA[...]]>
 *    The token contains the text between the opening and closing delimiters.
 *
 * TAGNAME:
 *    Starts with `<`.
 *    The opening `<` is consumed and excluded from the token.
 *    The token contains all subsequent characters up to, but not
 *    including, the first whitespace character.
 *    The terminating whitespace character is consumed.
 *    The scanner does not require `>` to terminate the tag name.
 *
 * CLOSE_TAG
 *    The closing tag name, beginning after `</` and ending at the
 *    first `>` or whitespace character.
 *    The opening `</` and terminating character are consumed.
 *    Delimiters are excluded from the token text.
 *
 * ATTR_NAME:
 *    Leading whitespace is consumed and discarded.
 *    The token contains all subsequent characters up to, but not
 *    including, the first `=` or whitespace character.
 *    The terminating character is consumed.
 *    An attribute name may be followed by whitespace, `=`, or both.
 *
 * ATTR_VALUE:
 *    If the first character is a single or double quote, that character
 *    is consumed and used as the terminating delimiter.
 *    The token contains all subsequent characters up to, but not
 *    including, the matching closing quote.
 *    The closing quote is consumed.
 *    Otherwise, the token contains all characters up to, but not
 *    including, the first whitespace character.
 *    The terminating whitespace character is consumed.
 *
 * CONTENT:
 *    The token contains all characters up to, but not including, the first
 *    `<` or `&` character.
 *    The terminating character is consumed.
 *
 *    Content may contain whitespace and arbitrary characters other than
 *    the terminating delimiters.
 *
 * ECODE:
 *    &...;
 *    The opening `&` is consumed.
 *    The token contains the text between `&` and `;`.
 *    The terminating `;` is consumed.
 * COMMENT:
 *     <!--...-->
 *     The token contains the text between the opening and closing delimiters.
 *
 * DIRECTIVE:
 *     <?...?>
 *     The token contains the text between the opening and closing delimiters.
 *
 * --General scanning behaviour--
 *
 * - The scanner operates on the input one character at a time.
 * - The opening and closing delimiters of a delimited section are consumed.
 * - Delimiters are excluded from the token text.
 * - A delimiter-based token may contain an empty string.
 * - The scanner does not perform entity expansion, escape processing,
 *   attribute interpretation, or other semantic processing.
 * - Whitespace is recognized only as the characters listed under WSPACE.
 * - Characters not explicitly recognized as delimiters are accepted as
 *   ordinary token content.
 *
 *
 * --Forbidden--
 *
 * Any input that cannot be scanned using the rules above will result in an
 * error. Examples include:
 *
 * - A missing closing delimiter for CDATA, COMMENT, or DIRECTIVE.
 * - A missing `;` terminating an ECODE.
 * - An unterminated TAGNAME, ATTR_NAME, ATTR_VALUE, or CONTENT token.
 * - An opening delimiter that does not match the expected delimiter.
 *
 * The scanner does not validate the resulting markup structure. Nesting,
 * matching tags, and attribute syntax are the responsibility of the parser.
 *
 */

enum xtree_gtml_token_id_t {
  xtree_gtml_token_SENTINEL     = XTREE_SCAN_TOKEN_SENTINEL,
  xtree_gtml_token_UNRECOGNISED = XTREE_SCAN_TOKEN_UNRECOGNISED,
  xtree_gtml_token_ERROR        = XTREE_SCAN_TOKEN_ERROR,
  xtree_gtml_token_EOF          = XTREE_SCAN_TOKEN_EOF,

  xtree_gtml_token_WSPACE,        // " \t\r\n\f"
  xtree_gtml_token_EQUAL,         // =
  xtree_gtml_token_GT,            // >
  xtree_gtml_token_SCLOSING,      // />
  xtree_gtml_token_CDATA,         // <![CDATA[...]]>
  xtree_gtml_token_TAGNAME,       // self-explanatory
  xtree_gtml_token_CLOSE_TAG,     // self-explanatory
  xtree_gtml_token_ATTR_NAME,     // self-explanatory
  xtree_gtml_token_ATTR_VALUE,    // self-explanatory
  xtree_gtml_token_CONTENT,       // self-explanatory
  xtree_gtml_token_ECODE,         // self-explanatory
  xtree_gtml_token_COMMENT,       // <!-- body -->
  xtree_gtml_token_DIRECTIVE,     // <? body ?>
};

#ifdef __cplusplus
extern "C" {
#endif

  // Get a string representation of the token id.
  const char *xtree_gtml_token_id_str (enum xtree_gtml_token_id_t id);

  // Scanning functions; use `xtree_scan_token_free()` to free the returned
  // values.
  xtree_scan_token_t *xtree_gtml_scan_wspace (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_equal (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_gt (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_cdata (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_tagname (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_close_tag (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_sclosing (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_attr_name (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_attr_value (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_content (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_ecode (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_comment (struct xtree_errobj_t *err, xtree_reader_t *reader);
  xtree_scan_token_t *xtree_gtml_scan_directive (struct xtree_errobj_t *err, xtree_reader_t *reader);

#ifdef __cplusplus
};
#endif


#endif


