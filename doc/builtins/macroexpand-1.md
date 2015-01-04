Builtin: macroexpand-1
----------------------

`(macroexpand-1` _expression_`)` => _expression_

Description
-----------

**macroexpand-1** takes a single expression as an argument and returns the
result of expanding the expression, if the first element of the expression is a
symbol representing a macro, or the expression itself otherwise.

In particular, if the first element of the expression is an expression other
than a symbol, this inner expression is not evaluated and **macroexpand-1**
returns the original expression, even if the inner expression might return a
macro, because the evaluation of an expression can have side effects.

Passing more or fewer than one argument results in a runtime error.

