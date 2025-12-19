
# Macro system
As stated previously Masfix has a very powerful [macro](https://en.wikipedia.org/wiki/Macro_(computer_science)) system.  
Although Masfix is whitespace insensitive we recommend using common indentation rules.

### Token lists
Token lists are simply groups of tokens grouped by matching brackets `()`, `[]` or `{}`.
The bracket types are fully interchangeable, but we recommend using common bracket conventions as we do in all examples.  
Token lists are used exclusivelly in the macro system's directives.

### Defining directives
Defining directives define a [constant](#directive-define) or a [macro](#macro-directive).  
They always start with `%` at the start of a line.

#### Define directive
The `%define` directive is a way to define a global numeric constant like this: `%define a_name 15`  
The constant can be used like `%a_name`, which replaces the expression with the constant value.

#### Macro directive
The `%macro` directive is a way to define a global parametrized macro like this:
```
%macro mac_name(x, y) {
	ld %x ; some body
	outur
	outc %y
}
```  
The macro can be used like `%mac_name(5, 8)` on a separate line.  
This replaces the expression with the macro body as text substitutuion.  
The macro arguments must be numerical-value-reducible expressions.  
Any occurences of macro parameters (`%x` and `%y` in this case) in the macro body will get replaced with the supplied values (here 5 and 8).  
