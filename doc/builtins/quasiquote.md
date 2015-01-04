Builtin: quasiquote
===================

`(quasiquote` _expression_`)` => _expression_

Description
-----------

**quasiquote** takes an expression as an argument and returns an interpretation
of the expression.

**quasiquote** is a special form. Parts of the expression are or are not
evaluated according to the following rules:
 - Atoms are returned unchanged.
 - Lists of the form `(unquote` _expression_`)` are replaced by the result of
   the evaluation of the inner expression.
 - Lists of the form `(unquote-splicing` _list_`)` are replaced in their
   containing lists by the result of the evaluation of the inner list.
 - All other lists are returned as lists, but only after each of their elements
   has been independently treated according to these rules.

This is the operator implied by use of `` ` ``.

`unquote` and `unquote-splicing` have no special meaning or value outside of
this context, and any values assigned to them are ignored in this context.

Passing more or fewer than one argument results in a runtime error.

Lists starting with `unquote` or `unquote-splicing` and having more or fewer
than one additional expression result in a runtime error.

Lists starting with `unquote-splicing` but not contained in an outer list
result in a runtime error.

Lists starting with `unquote-splicing` whose additional expression is not a
list result in a runtime error.

