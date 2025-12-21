## Implementation notes for compile time directives

	- 2 things: leaving ctime exp while preprocessing / parsing
	- leaving expansion in TS / leaving it open in Scope
	- DONT CONFUSE WITH ARGLIST
	- LIGHT version:
		- IDEAL: (ctime expansion never added to TS)
			- practically is added, but meeting it is not happening, due to no module parsing is happening
		
		- STEP BY STEP
		- expandMacroUse:
			- preprocesses arglist - eats it from TS!
				- processArglistWrapper
					- opens arglist itr
					- preprocess ctime arglist
					- closes itr 
			- inserts macro expansion to TS, opens itr
		- preprocessing ctime body
		- closeList closes ctime's iteration in scope after preprocessing
		- forceParse reopens itr
			- forceParse: parse ctime body, remember asm instr start
		- forceParse closes parsing itr
		- parseInterpretCtime
			- interprets ctime
			- removes ctime expansion from TS!
			- inserts retval to TS
			- removes ctime's asm instrs from parseCtx

	- HARD version:
		- added in expandMacroUse to TS
		- preprocess insides
		- parse module before, ctime body
		- when does new closeList remove ctime expansion token??
