Calypso Syntax
==============

Generally speaking, Calypso aims to follow the conventions of modern Lisp
syntax.

Use of the ASCII character set is assumed in all cases.

Overview
--------

 - A **sequence** is an ordered collection of one or more elements.

   - Adjacent elements in a sequence _must not_ be separated by anything unless
     explicitly specified otherwise.

 - A **program** is a sequence of expressions, optionally separated by
   whitespace.

   - A program need not be contained within a single file; in this sense, even
     library initialization code is part of the program.

 - An **expression** is either an S-expression or an atom.

S-expressions
-------------

 - An **S-expression** is a sequence consisting of an `(`, an optional sequence
   of expressions, an optional `.` and expression, and an `)`.

   - All elements of an S-expression _can_ be separated by whitespace.

   - If the second component is omitted, it is equivalent to if the component
     had been included and consisted of the atom `nil`.

   - If the third component is omitted, it is equivalent to if the component
     had been included and its expression was the atom `nil`.

   - An S-expression is nominally a linked list. However, the `.` syntax allows
     for specifying a value for what would normally be the NULL (nil) pointer
     signifying the end of the list (the cdr of the last cons cell); this means
     an S-expression can represent not just lists but trees.

Atoms
-----

 - An **atom** is a symbol, an integer, a real, a character, or a string.

   - Adjacent atoms in a sequence _must_ be seperated by whitespace.

 - A **symbol** is a sequence consisting of an upper or lowercase letter or `$`
   or `_`, followed by zero or more upper or lowercase letters or digits or `$`
   or `_` or `-`.

   - As special cases, `=`, `+`, and `-` are symbols when it is not possible
     for them to be a component of another atom.

   - The same symbol input syntax will always correspond to the same symbol
     internally, but note that it is possible for two distinct internal symbols
     to have the same print representation (i.e., output syntax).

 - An **integer** is a sequence consisting of an optional sign, followed by a
   sequence of decimal digits, or `0` and a sequence of octal digits, or `0x`
   and a sequence of hexadecimal digits.

   - The range of representable values is that of a 64-bit signed integer using
     a twos-complement representation.

 - A **real** is a sequence consisting of an optional sign, followed by `inf`,
   or `nan`, or a sequence of decimal digits and a `.` and zero or more decimal
   digits, or a `.` and a sequence of decimal digits, followed by an optional
   exponent.

   - The range of representable values is that of a double-precision IEEE 754
     floating-point number.

 - An **exponent** is a sequence consisting of an `E` or an `e`, followed by an
   optional sign and a sequence of decimal digits.

 - A **sign** is a `+` or a `-`.

 - A **character** is a sequence consisting of a `'`, a byte (excluding `'` and
   `\`) or an escape sequence, and a `'`.

 - A **string** is a sequence consisting of a `"`, a sequence of bytes
   (excluding `"` and `\`) or escape sequences, and a `"`.

 - An **escape sequence** consists of a `\`, followed by a sequence of octal
   digits, or an `x` and a sequence of hexadecimal digits, or a single byte
   other than an octal digit or an `x`.

   - Escaped octal and hexadecimal sequences are evaluated mod 256 (i.e., the
     lowest-order byte is used).

   - For other escape sequences, if the single byte is `a`, `b`, `f`, `n`, `r`,
     `t`, or `v`, the sequence evaluates to ASCII alarm, backspace, formfeed,
     newline, carriage return, horizontal tab, or vertical tab, respectively.
     Otherwise, the escape sequence evaluates to the single byte itself.

Quoting Syntax
--------------

 - The sequence `'x`, where `x` represents an expression, is equivalent to the
   expression `(quote x)`.

 - The sequence `` `x``, where `x` represents an expression, is equivalent to
   the expression `(quasiquote x)`.

 - The sequence `,x`, where `x` represents an expression, is equivalent to the
   expression `(unquote x)`.

 - The sequence `,@x`, where `x` represents an expression, is equivalent to
   the expression `(unquote-splicing x)`.

