
#ifndef H_XTREE_SCAN
#define H_XTREE_SCAN

typedef struct xtree_scan_token_t xtree_scan_token_t;

typedef xtree_scan_token_t *(xtree_scan_fptr_t) (xtree_reader_t *reader);

/* ***********************************************************************
 * NOTE: Token IDs less than or equal to zero are reserved. While all IDs less
 * than or equal to zero are reserved, only the following specific IDs are
 * recognised.
 *    id == 0:   EOF
 *    id == -1:  Error
 *    id == -2:  Sentinel
 */

#define XTREE_SCAN_TOKEN_EOF          (0)
#define XTREE_SCAN_TOKEN_ERROR        (-1)
#define XTREE_SCAN_TOKEN_SENTINEL     (-2)


#ifdef __cplusplus
extern "C" {
#endif

  xtree_scan_token_t *xtree_scan_token_new (int id,
                                            const char *filename,
                                            size_t line, size_t cpos,
                                            const unsigned char *text, size_t len);
  void xtree_scan_token_free (xtree_scan_token_t **token);

  int xtree_scan_token_id (const xtree_scan_token_t *token);
  void xtree_scan_token_id_set (xtree_scan_token_t *token, int id);
  const unsigned char *xtree_scan_token_text (const xtree_scan_token_t *token);
  const char *xtree_scan_token_filename (const xtree_scan_token_t *token);
  size_t xtree_scan_token_line (const xtree_scan_token_t *token);
  size_t xtree_scan_token_cpos (const xtree_scan_token_t *token);

  // Frees and reallocates `dst` to hold the printed representation of the
  // token in `field: value` format.
  unsigned char *xtree_scan_token_dump (const xtree_scan_token_t *token,
                                        unsigned char **dst);


  void xtree_scan_token_errv (struct xtree_errobj_t *err,
                              xtree_scan_token_t *token,
                              enum xtree_errcode_t errcode,
                              const char *fmts,
                              va_list ap);

  void xtree_scan_token_err (struct xtree_errobj_t *err,
                             xtree_scan_token_t *token,
                             enum xtree_errcode_t errcode,
                             const char *fmts,
                             ...);

  bool xtree_scan_token_id_assertv (struct xtree_errobj_t *err,
                                    xtree_scan_token_t *token,
                                    int id, va_list ap);

  bool xtree_scan_token_id_assert (struct xtree_errobj_t *err,
                                   xtree_scan_token_t *token,
                                   int id, ...);

#ifdef __cplusplus
};
#endif


#endif


