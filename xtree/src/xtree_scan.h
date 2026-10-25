
#ifndef H_XTREE_SCAN
#define H_XTREE_SCAN

typedef struct xtree_scan_token_t xtree_scan_token_t;

typedef xtree_scan_token_t *(xtree_scan_fptr_t) (xtree_reader_t *reader);

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


#ifdef __cplusplus
};
#endif


#endif


