Builtin: cons
=============

`(cons` _expression_ _expression_`)` => _list_

Description
-----------

**cons** takes two expressions as arguments and returns a newly-allocated cons
cell referencing the first and second expressions, in order.

S-expressions are singly-linked lists constructed from cons cells, where the
first field of each cell contains a reference to an element of the list, and
the second field contains a reference to the next cell of the list. A reference
to the empty list signals the end of the S-expression.

Passing more or fewer than two arguments results in a runtime error.

