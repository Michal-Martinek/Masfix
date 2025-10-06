import gdb
import json

def to_json(expr):
    val = gdb.parse_and_eval(expr)
    # You must write code to traverse your AST and build a Python dict
    ast_dict = {
        "type": str(val.type),
        "value": str(val)
        # ... add more fields as needed ...
    }
    print(json.dumps(ast_dict))

gdb.define_command('to_json', to_json)
