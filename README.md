**Pipelight is an interactive simulator built with Qt/C++ that visualizes the out-of-order execution of Assembly code.

---

## ✅ Supported Features & Instruction Set

Pipelight simulates a rich subset of the x86 instruction set architecture, focusing on the core operations needed to demonstrate the principles of out-of-order execution.

| Category | Supported Instructions | Example Formats |
| --- | --- | --- |
| **Arithmetic** | `ADD`, `SUB`, `MUL`, `DIV`, `INC`, `DEC` | `ADD RAX, RBX, RCX` <br> `SUB RDX, RDX, 50` |
| **Logical** | `AND`, `OR`, `XOR`, `NOT` | `AND RAX, RBX` <br> `XOR R8, R8, R8` |
| **Data Transfer**| `MOV`, `LEA` | `MOV RAX, 100` <br> `MOV RCX, RDX` <br> `LEA R8, [RAX+100]`|
| **Memory** | `LOAD`, `STORE` | `LOAD RAX, [RSP+0]` <br> `STORE RBX, [RBP+16]`|
| **Stack** | `PUSH`, `POP` | `PUSH RAX` <br> `POP RBX`|
| **Control Flow** | `CMP`, `JMP`, `CALL`, `RET` <br> `JZ`, `JNZ`, `JG`, `JGE`, `JL`, `JLE`| `CMP RAX, 100` <br> `JMP loop_label` <br> `JZ equals_label`|

The parser is **case-insensitive** and correctly handles labels and inline comments (`;`).

---
