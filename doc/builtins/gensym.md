Builtin: gensym
===============

`(gensym)` => _symbol_

Description
-----------

**gensym** takes no arguments and returns a symbol that is guaranteed to be
distinct from every other symbol.

The symbol will have a print representation of the form `Gnnnnn`, where `nnnnn`
is a zero-padded decimal number that increases each time gensym is called.
However, even though this counter can wrap around, every invocation of gensym
always generates a symbol that will not compare equal (in the `eq` sense) to
any other symbol.

Passing any arguments results in a runtime error.

