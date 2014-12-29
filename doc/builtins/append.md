Builtin: append
===============

`(append` _list_*`)` => _list_

Description
-----------

**append** takes zero or more lists as arguments, and returns their
concatenation.

The concatenation consists of shallow copies of all but the last argument; the
last agrument is itself used as the end of the resulting list.

Passing zero arguments produces the empty list.

Passing non-list arguments results in a runtime error.

