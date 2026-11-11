
#ifndef H_XTREE_GTML_PARSE
#define H_XTREE_GTML_PARSE

/*
 * xtree_gtml_parse(): parse one GTML-like value from a reader into an xtree.
 *
 * TODO: Specify the language grammer here.
 */

#ifdef __cplusplus
extern "C" {
#endif

  xtree_node_t *xtree_gtml_parse (struct xtree_errobj_t *err,
                                  xtree_reader_t *reader,
                                  size_t max_depth);

  // Convert a GTML that was parsed into an xtree_node_t back into a GTML
  // string. The caller is responsible for freeing the returned value.
  char *xtree_gtml_parse_dump (struct xtree_errobj_t *err,
                               const xtree_node_t *tml_node,
                               size_t indent);


#ifdef __cplusplus
};
#endif


#endif


