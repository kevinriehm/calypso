Builtin: quote
==============

`(quote` _expression_`)` => _expression_

Description
-----------

**quote** takes an expression as an argument and returns the expression without
evaluating it.

**quote** is a special form. Its argument is never evaluated.

This is the operator implied by use of `'`.

Passing more or fewer than one argument results in a runtime error.

