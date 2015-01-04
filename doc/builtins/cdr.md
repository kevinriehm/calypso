Builtin: cdr
============

`(cdr` _list_`)` => _expression_

Description
-----------

**cdr** takes one non-empty list as an argument and returns the second
expression pointed to by the first cons cell of the list.

This essentially means the rest of the list after the first element, but if the
list is considered as a tree, then this also means the right sub-tree.

Passing more or fewer than one argument results in a runtime error.

Passing the empty list or an atom results in a runtime error.

