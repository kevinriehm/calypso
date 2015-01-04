Builtin: eq
============

`(eq` _expression_ _expression_`)` => `t` or `nil`

Description
-----------

**eq** takes two expressions as arguments and returns `t` if they refer to the
same object, or `nil` otherwise.

Specifically:
 - Expressions of different types are not the same.
 - Lists are the same if they refer to the same cons cell.
 - Symbols are the same if they refer to the same internal symbol.
 - Integers are the same if they are equal numerically.
 - Reals are the same if they are equal numerically.
 - Characters are the same if they are the same character.
 - Strings are the same if they refer to the same in-memory string.
 - Built-in functions are the same if they refer to the same builtin.
 - Lambdas are the same if they refer to the same in-memory lambda.

Passing more or fewer than two arguments results in a runtime error.

