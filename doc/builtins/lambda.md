Builtin: lambda
===============

`(lambda (`_argument-template_ (`.` _symbol_)?`)` _expression_*`)` => _lambda_

argument-template: _symbol_
                 | _argument-template_*
                 | `(`_argument-template_ (`.` _symbol_)?`)`

Description
-----------

**lambda** takes a structured tree of symbols and a list of zero or more
expressions and returns a lambda that takes arguments according to the given
template, evaluates the list of expressions as its body when it itself is
evaluated, and has the current lexical environment as its containing closure.

**lambda** is a special form. The argument list is never evaluated as an
expression, and the expressions given as the lambda body are evaluated only
when the resultant lambda itself is evaluated.

Lambdas themselves are also special forms. Their arguments are only evaluated
when they are being bound to a symbol from the argument template; i.e., if the
template specifies a list with three symbols, the lambda must be invoked with a
syntactical list containing three expressions, not an expression that evaluates
to a list of three expressions. This allows convenient deconstruction of a
syntactically-layered argument structure.

Of particular note is the optional `.` _symbol_ component at the end of any
list in the argument template. If a symbol is provided here, it will be bound
upon invocation to a list of all arguments provided in excess of those
explicitly specified; all of these excess arguments will still be evaluated.
Thus, with this component present, an arbitrarily large number of arguments can
be accepted (but not fewer than the number of explicitly specified arguments);
without this component, there _must not_ be any excess arguments at the
corresponding level in the arguments.

When a lambda is invoked, its return value is the return value of the last body
expression.

Passing zero arguments results in a runtime error.

Passing zero body expressions produces a lambda which always returns the empty
list (but each argument is still evaluated and checked against the template).

Invoking a lambda with a mis-matched set of arguments results in a runtime
error.

