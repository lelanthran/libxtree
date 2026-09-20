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

**Intelligent** parsing has the advantage of adapting to almost any sort of
tree (for example reading Java or C++ source code into a tree), with a
downside of complexity in writing adaptors.

OTOH, **Dumb** parsing has the advantage of less complexity in writing parsers
but with the disadvantage of only allowing simpler tree formats (not sure if
it will even be possible to represent programming source code).

> Note that **dumb** parsing can probably do trees if the tree itself has a
> caller-specified id/type; adaptor/parser will then return a tuple of
>       { token_type, token_id, token_text }
> So that a variable can be returned as { TOKEN_NODE, SYMBOL, "i" }, with
> a variable declaration `static volatile char * s` returned as 5 nodes with
> token types set to symbol, symbol, symbol, ptr, symbol. This is not ideal
> and will require a further transformation before it is an actual AST.

# Sample: Option `Intelligent`

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

