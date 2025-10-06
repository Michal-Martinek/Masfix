import gdb
import json

class ToJsonCommand(gdb.Command):
    """Convert an expression to JSON."""

    def __init__(self):
        super(ToJsonCommand, self).__init__("to_json", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        val = gdb.parse_and_eval(arg)
        # You must write code to traverse your AST and build a Python dict
        ast_dict = {
            "type": str(val.type),
            "value": str(val)
            # ... add more fields as needed ...
        }
        print(json.dumps(ast_dict))

ToJsonCommand()
