
# Macro system
As stated previously Masfix has a very powerful [macro](https://en.wikipedia.org/wiki/Macro_(computer_science)) system.  
The macro system machinery is not directly tied to assembly instructions, it's purpose is to manipulate token flow, from which instructions are parsed.  
All macro system directives begin with `%` (or `!`).  
_Although Masfix is whitespace insensitive we will use common indentation rules._

### Token lists
Token lists are simply groups of tokens used by the macro system.
Tokens are grouped by matching bracket pairs - `()`, `[]` or `{}`.  
_The bracket types are meaningless._

### Directive names
Allowed directive names consist of letters or `_`.  
No name can overwrite an instruction opcode and identifier redefinition rules apply.

## Defining directives
Defining directives define a [constant](#directive-define), a [macro](#macro-directive) or [namespace](#namespaces).  
Directive name can have complex nature similar to [assembly instructions](./assembly.md#complex-identifiers) - tokens wrapped in `()`.  
- _example:_ `%macro (name_(%a)) (arg1, arg2) { outu 6 }`

#### Define directive
The `%define` directive defines a numerical constant - either literal or expr wrapped in braces.  
Using the constant replaces the expression with the constant value.
```asm
%define a 15
lda %a ; -> lda 15

; define a constant with name=complex_value_0, value=21
%define (
		(complex)( _ )value_(!stack:top()) ; top of VM stack
	) (
		!op(a, %a, 6) ; math:op adds 6 to %a
)
outu %complex_value_0 ; 21
```
  
_Name and value fields must start inline._

### Macro directive
The `%macro` directive defines a parametrized macro.  
Two [token lists](#token-lists) are supplied:
1) parameter list - comma separated argument names, must be inline
1) macro body - token list, not parsed in any sense
	- may contain other directives (including macro uses), defining directives in body is forbidden
```
%macro mac_name(x, y) {
	ld %x    ;\
	outur    ; macro body
	outc %y  ;/
}
```

#### Expanding macros
Using a macro involves passing proper number of arguments.  
Macro arguments may be basically any tokens (even macro expansions!), any macro system directives in argument list are expanded before expanding the macro.  

```asm
%mac_name(5, 8)
; expands to:
ld 5   ; %x
outur
outc 8 ; %y
```
Macro use gets replaced with the macro body as text substitutuion,  
although macro body experiences the namespace in which macro was defined.  
Occurences of macro parameters (`%x` and `%y` above) are replaced with the supplied arguments (5 and 8).  

### Namespaces
Namespace is place in which [defines](#define-directive) and [macros](#macro-directive) live.  
They can be nested and are useful for splitting different functionality or API / implementation.  
Code execution is unaffected by namespaces and comes through them.  

```asm
%namespace huzzaah {
	%define baro 99
	%macro seeBaro() {
		outu %baro
	}
}
%baro ; hidden inside - error
%huzzaah:seeBaro() ; reaches inward for the macro
```

#### Namespace structure
Namespaces form a tree-like structure.  
Higher level namespaces are accessible, others can be accessed with `%namespace1:space2:identifier` syntax.  

#### using directive
The `%using` directive makes all identifiers in another namespace accessible from the current one.
```asm
%include "memory"
%stack:push(1)
%using stack:impl ; defined in std/core/memory
%derefSPpop() ; from memory:impl
```

#### include directive
The `%include` directive includes code from different Masfix file.  
Syntax: `%include "path/to/code.mx"`  
Include paths can be customized with compiler's `--include` option.  
Included code is placed atop the current file.  
Repeated including of the same file is safe and ignored.  

## Compile time macro use
Enable use of assembly functionality (like arithmetics and IO) inside the preprocessor.  
Macro uses can be used in compile time mode by using `!` instead of `%`.  
1) ctime macro's body is fully preprocessed to instruction flow as usual
2) their body is executed in special compile-time VM
3) content of `r` register is used as return value, which is inserted as numerical token instead of the macro use
- _Ctime machinery explained in great detail [here](impl_ctime-machinery-desc.md)_

Example:
```asm
%macro add(a, b) {
	ld %a
	lda %b
	; return value taken from r
}

mov !add(2, 7)
; expands & processes the macro body into instructions
; executes them inside VM
; value in r after excution of the body is 9
; numerical token '9' is pushed in place of macro use, becoming:
mov 9
```

Potential nested ctime uses are evaluated from the inside.  
