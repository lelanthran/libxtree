
#ifndef H_XTREE_EMITTER
#define H_XTREE_EMITTER

// Read `xtree.h` in conjunction with this document. The documentation in
// `xtree.h` gives a good high-level overview of the goal of this project.
//
// This document presents the detail required to parse new tree formats.
//
// In short, the `xtree_node_t` type is intended to be a common representation
// format for trees. It is intended to represent most tree formats (Directed
// Acyclic Graphs) in a common representation. The "adaptors" referred to in
// the documentation in `xtree.h` are the different formats, such as JSON,
// XML, markdown (also a tree format), s-expressions, ASTs, etc.
//
// To implement an adaptor you must write a function that takes an
// `xtree_emitter_t` object, and emit events through that object (events
// documented more throughly below). The emission of events matches the
// parsing of a tree. For example:
//  - START_NODE:   Adaptor found a new node
//  - CONTENT:      Adaptor found content
//  - KEY-VALUE:    Adaptor found a key-value pair
//  - ...
//
// The xtree_emitter_t object stores state (current node, current line, current
// source name, etc), allows mutation of the current node (as the adaptor
// parses), allows returning to a parent node (when the adaptor parses an
// end-of-node token, such as `</name>` for HTML or `}` for JSON, etc), stores
// the state and depth of the tree, etc.
//
// The adaptor need not store state like the current node, nor track
// recursively descending or ascending the tree, select the input source, etc.
//
// In brief, the adaptor parses the format, parsing the data and sending
// parse-events using an `xtree_emit_t` object. The `xtree_emitter_t` object
// receives these events and uses them to build up the `xtree_node_t` in
// memory.
//
// One unusual behaviour in this flow is that functions attached to the
// `xtree_emit_t` object may not return: the emitter may encounter an error
// that prevents further parsing (such as OOM situations). This means that the
// adaptor has to be a clean function that does not acquire resources
// outside of the emitter. For memory allocation, reallocation and free, use the
// `xtree_emit_t` functions `->malloc()`, `->calloc()`, `->realloc()` and
// `->free()` (list not complete in the comments, check the declarations in
// the header; other functions include getc, etc for getting the next
// character).
//

// The caller uses the emitter handle (setup, diagnostics, etc)
typedef struct xtree_emitter_t xtree_emitter_t;

// No constructor or destructor for this structure. Caller (the adaptor) will
// populate the fields and maintain ownership of them as well.
struct xtree_sslice_t {
  const char *source;
  size_t startpos;
  size_t endpos;
};


// All adaptors must match this function; returns true on success and false on
// failure. Errors abort the function.
// typedef bool (struct xtree_adaptor_fptr_t) (xtree_emit_t *emit);


#ifdef __cplusplus
extern "C" {
#endif

// These are the first set of functions, intended for the caller to construct
// and set up an emitter. The adaptor will never call these functions.

// Construct an emitter that must be freed with `xfree_emit_free()`.
xtree_emitter_t *xtree_emitter_new (void);

// Free the specified emitter. If `emitter` or `*emitter` is NULL, no action is
// taken.
void xtree_emitter_free (xtree_emitter_t **emitter);

// For diagnostics and testing, dump the state. If outf is NULL, then stdout is
// used.
void xtree_emitter_dump (const xtree_emitter_t *emitter, FILE *outf);

// Start the specified adaptor.
// This function returns:
//  1:    Adaptor started, but signalled an error during parsing, error message
//        can be retrieved from the emitter.
//  0:    Adaptor started and ran to completion successfully
//  -1:   Adaptor started, but encountered tree errors
// int xtree_emitter_parse (xtree_emitter_t *emitter, xtree_adaptor_fptr_t *fptr);



// Get the `xtree_emit_t` object that a handler can use to emit parse-events.
// This cannot be freed directly. It is freed when the containing
// `xtree_emitter_t` is freed.
// xtree_emit_t *xtree_emitter_get (xtree_emitter_t *emitter);


// These functions will be called by an adaptor. Note that they may not
// return as errors will cause the emitter to return directly to its caller.



#ifdef __cplusplus
};
#endif


#endif


