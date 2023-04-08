# disasm_2023
disassembler project 

## Usage
### Compile
```g++ main.cpp disasm.h -fpermissive -o  disasm```

### Run
Each file **must** have a valid extension (eg. ``echo.bin``)
- files:
```./disasm <file> [<fileout>] [mode]```
- sample:
```./disasm othercore.bin```

- single instruction:
```./dthumb <code> [mode]```
`code` needs to be written in hexadecimal format (see sample)
- sample:
```./disasm  1AC0```

### Stat
Now uses by default file ``out.txt``
```python3 instruction_stats.py```
