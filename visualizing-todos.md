- add to `settings.json`
```
    "debugVisualizer.js.customScriptPaths": [
    "${workspaceFolder}/tokenExtractor.js"
    ],
    "debugVisualizer.debugAdapterConfigurations": {
            "gdb": {
                "expressionTemplate": "script to_json(\"scope.modules.begin()->contents\")",
                "context": "repl"
            },
            "lldb": {
                "expressionTemplate": "script to_json(\"scope.modules.begin()->contents\")",
                "context": "repl"
            }
    },
```

- add to .gdbinit ?
source C:/Users/micha/source/Langs/Masfix/gdb_ast_to_json.py

- npm install @hediet/debug-visualizer-data-extraction