Builtin: macroexpand
====================

`(macroexpand` _expression_`)` => _expression_

Description
-----------

**macroexpand** takes a single expression as an argument and returns the result
of repeatedly expanding the expression as long as the first element of the
expression is a symbol representing a macro.

In particular, if the first element of the expression is an expression other
than a symbol, this inner expression is not evaluated and **macroexpand**
terminates, even if it might return a macro, because the evaluation of an
expression can have side effects.

Passing more or fewer than one argument results in a runtime error.

