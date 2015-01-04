Builtin: cond
=============

`(cond (` _expression_ _expression_ `)`*`)` => _expression_

Description
-----------

**cond** takes zero or more length-two lists as arguments and, for each list in
sequence, returns the second expression if the first expression does not return
the empty list.

**cond** is a special form. Only the minimum number of expressions are
evaluated: the first expression of each list until the first such one that does
not return the empty list, and that expression's corresponding second
expression.

If all the tested expressions return the empty list, then **cond** returns the
empty list.

Passing zero arguments produces the empty list.

Passing arguments not of the form specified results in a runtime error.

