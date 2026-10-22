
#ifndef H_XTREE_READER
#define H_XTREE_READER

typedef struct xtree_reader_t xtree_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

  // Construct a new reader object from a string or a file. In the case of the
  // string, the reader makes a copy of it so the caller can mutate the
  // parameter `input` after the `xtree_reader_str()` function returns.
  // **NOTE**: The construction from a file is not yet supported.
  xtree_reader_t *xtree_reader_str (const char *name, const char *input);
  // xtree_reader_t *xtree_reader_file (const char *name, FILE *input);

  // Delete a reader object, freeing all resources held by it.
  void xtree_reader_free (xtree_reader_t **reader);

  // Retrieve specific states and statuses from the reader. The caller must
  // not mutate the returned errobj_t structure. When there is no error, NULL
  // is returned.
  const struct xtree_errobj_t *xtree_reader_error (const xtree_reader_t *reader);
  bool xtree_reader_eof (const xtree_reader_t *reader);
  size_t xtree_reader_line (const xtree_reader_t *reader);
  size_t xtree_reader_cpos (const xtree_reader_t *reader);

  // Get the next character (advancing the position of the reader), unget the
  // next character (maintaining the correct line number and character
  // position) and peek at the next character without updating the position of
  // the reader.
  //
  // Note that unget only works once; behaviour is undefined is unget is
  // called multiple times without an intervening get().
  int xtree_reader_get (xtree_reader_t *reader);
  void xtree_reader_unget (xtree_reader_t *reader);
  int xtree_reader_peek (xtree_reader_t *reader);


#ifdef __cplusplus
};
#endif


#endif


