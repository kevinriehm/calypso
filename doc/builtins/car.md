Builtin: car
============

`(car` _list_`)` => _expression_

Description
-----------

**car** takes one non-empty list as an argument and returns the first
expression pointed to by the first cons cell of the list.

This essentially means the first element of the list, but if the list is
considered as a tree, then this also means the left sub-tree.

Passing more or fewer than one argument results in a runtime error.

Passing the empty list or an atom results in a runtime error.

