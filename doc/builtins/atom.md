Builtin: atom
=============

`(atom` _expression_`)` => `t` or `nil`

Description
-----------

**atom** takes one expression as an argument and returns `t` if the expression
is an atom or `nil` otherwise.

Note that the empty list is also an atom, and therefore `(atom ())` returns
`t`.

Passing more or fewer than one argument results in a runtime error.

