# NOTES

What should an adaptor look like? There are two options:
1. **Intelligent**
  The adaptor does all the parsing, all the decision-making, etc and calls
  back into the xtree library with events indicating instructions to emit,
  much in the same way a compiler works.
2. **Dumb**
  The adaptor provides a single `parse_next` function that returns the next
  token every time it is called. The xtree library constructs the tree
  recursively using these tokens.

> Note that **dumb** parsing can probably do trees if the tree itself has a
> caller-specified id/type; adaptor/parser will then return a tuple of
>       { token_type, token_id, token_text }
> So that a variable can be returned as { TOKEN_NODE, SYMBOL, "i" }, with
> a variable declaration `static volatile char * s` returned as 5 nodes with
> token types set to symbol, symbol, symbol, ptr, symbol. This is not ideal
> and will require a further transformation before it is an actual AST.

**Intelligent** parsing has the advantage of adapting to almost any sort of
tree (for example reading Java or C++ source code into a tree), with a
downside of complexity in writing adaptors.

OTOH, **Dumb** parsing has the advantage of less complexity in writing parsers
but with the disadvantage of only allowing simpler tree formats (not sure if
it will even be possible to represent programming source code).

# Sample: Option `Intelligent`
Both parsing and tokenisation logic live in the adaptor function.
```c

bool adaptor_func (libctx_t *ctx, xtree_rxer_t *rxer)
{
  // ...
  while (parse_token (handle, &token)) {
    switch (token.type) {

      case token_NODE:
        emit (ctx, rxer, NODE_START, token);
        break;

      case token_ATTR:
        parse_attr_name (handle, &name_token);
        parse_attr_value (handle, &value_token);
        emit (ctx, rxer, ADD_ATTR, &name_token, &value_token);

      case token_CONTENT:
        emit (ctx, rxer, ADD_CONTENT, content_token);

    }
  }
  // ...
}
```

# Sample: Option `Dumb`
The adaptor does scanning only, producing tokens.
```c

bool xtree_parse (adaptor_parser_t *fptr)
{
  // ...
  while (fptr (input)) {
    switch (token.type) {

      case token_NODE: // Construct a new node, make it current
      case token_ATTR_NAME: // Construct a new attribute, make it current
      case token_ATTR_VALUE: // Add value to current attribute
    }
  }
}
```

# Hybrid (normal)

The other problem with the **dumb** option is that the parser function will
routinely need to track state: For example `<tag>` must be returned as a new
node, but not if it is within a cdata block.

Maybe give the adaptor a state field they can write into, to track their own
state? Then each call to an `adaptor_token_next` function will include
parameters for `state`. Adaptor could populate a single struct as follows:
```
struct {
  event_t  event;       // NODE_START, etc
  int      token_type;  // adaptor-controlled; TSYMBOL, TQUALIFIER, etc
  int      state;       // adaptor-controlled; name of state in state-machine
  char    *text;        // Text that was parsed
  size_t   textlen;     // Exact length of text
  err_t    errcode;     // xtree enum
  char     errmsg[64];  // error message
  char     warning[64]; // Emits a diagnostic, but continues processing.
};
```

The input to the parsing function would be `getc` and `ungetc` type functions.
