Builtin: macro
==============

`(macro (`_argument-template_ (`.` _symbol_)?`)` _expression_*`)` => _macro_

argument-template: _symbol_
                 | _argument-template_*
                 | `(`_argument-template_ (`.` _symbol_)?`)`

Description
-----------

**macro** takes a structured tree of symbols and a list of zero or more
expressions and returns a macro (a type of lambda) that takes arguments
according to the given template, evaluates the list of expressions as its body
to produce an expression for futher evaluation, and has the current lexical
environment as its containing closure.

**macro** is a special form. The argument list is never evaluated as an
expression, and the expressions given as the macro body are evaluated only when
the resultant macro itself is evaluated.

Macros themselves are also special forms. Their arguments are never evaluated
as such: instead, their syntactical forms are bound directly, as specified by
the argument template. Additionally, when a macro is evaluated, its body
expressions are evaluated as with a normal lambda, but then the return value of
the final body expression is itself evaluated, and the return value of this
final expression is the ultimate return value of the original macro expression.

In all other respects, **macro** and macros operate as **lambda** and normal
lambdas do.

Passing zero arguments results in a runtime error.

Passing zero body expressions produces a macro which always returns the empty
list (but each argument is still checked against the template).

Invoking a macro with a mis-matched set of arguments results in a runtime
error.

