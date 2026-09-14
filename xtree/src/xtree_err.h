
#ifndef H_XTREE_ERR
#define H_XTREE_ERR

enum xtree_errcode_t {
  xtree_errcode_NONE = 0,
  xtree_errcode_OOM,
  xtree_errcode_PARAMETER_TYPE,
  xtree_errcode_ATTRIBUTE_NOT_FOUND,
  xtree_errcode_OUT_OF_BOUNDS,
  xtree_errcode_INTERNAL_STATE,
  xtree_errcode_CHILD_NOT_FOUND,
  xtree_errcode_NULL_PARAMETER,
};

struct xtree_errobj_t {
  int syscode;
  char sysmsg[64];

  enum xtree_errcode_t libcode;
  char libmsg[64];

  char srcfile[64];
  int srcline;

  char *message;
  size_t message_len;
};

#define ERROR(e,c,...)      do { \
  e->syscode = errno; \
  strncpy (e->sysmsg, strerror (errno), (sizeof e->sysmsg) - 1); \
  e->sysmsg[(sizeof e->sysmsg) - 1] = 0; \
  e->libcode = c; \
  strncpy (e->libmsg, xtree_errcode_str (c), (sizeof e->libmsg) - 1); \
  e->libmsg[(sizeof e->libmsg) - 1] = 0; \
  strncpy (e->srcfile, __FILE__, (sizeof e->srcfile) - 1); \
  e->srcfile[(sizeof e->srcfile) - 1] = 0; \
  e->srcline = __LINE__; \
  snprintf (e->message, e->message_len, __VA_ARGS__); \
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

  // Turn an errorcode into a string
  const char *xtree_errcode_str (enum xtree_errcode_t code);

  // Reset the message buffer to the specified size. Use `0` to free it
  // completely. NOTE: This just resizes (or frees) the message buffer. Use
  // xtree_errobj_clrerr() to clear the errors stored.
  bool xtree_errobj_reset (struct xtree_errobj_t *err, size_t msglen);

  // Clear the error object. Does not free the message, only clears it. Use
  // `reset` with a message-length of `0` to free it (or do it manually, as
  // the fields are exposed to the caller). NOTE: This only clears the error
  // stored, it does not free any buffers. To free the message buffer, use
  // xtree_errobj_reset with a parameter of `0` for the msglen.
  void xtree_errobj_clrerr (struct xtree_errobj_t *err);

  // Print the error object. If `outf` is NULL, `stdout` is used.
  void xtree_errobj_dump (const struct xtree_errobj_t *err, FILE *outf);

#ifdef __cplusplus
};
#endif


#endif


