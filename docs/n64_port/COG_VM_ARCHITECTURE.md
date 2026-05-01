# COG Virtual Machine Architecture

The COG VM is the scripting engine for Jedi Knight: Dark Forces II. It is responsible for all level logic, events, switches, doors, and complex weapon behaviors. For the N64 port, the parser must be stripped out, and the runtime execution engine optimized for the VR4300 CPU.

## Core Components (`src/Cog/`)

### 1. The Script Representation (`sithCog`)
A `sithCog` struct represents a running instance of a script.
- **Variables/Parameters:** COGs have local variables (`desc` table) which hold integers, floats, vectors, or object references (Things, Sectors, Surfaces).
- **Triggers:** A COG defines specific event hooks (e.g., `activated`, `entered`, `timer`, `killed`). When the engine registers one of these events, it calls into the VM to execute the corresponding label.

### 2. Execution Engine (`sithCogExec.c`)
- **Stack-Based VM:** The VM uses a custom execution stack (`sithCogExec_PushInt`, `sithCogExec_PopVector`, etc.).
- **Bytecode:** Instructions are defined by opcode enumerations. On the N64, `switch/case` dispatch or computed gotos (if supported by `libdragon` GCC) should be evaluated for interpreter speed.
- **State Management:** The engine allows for sleeping (`Sleep()`) and timers. This is managed by yielding execution and saving stack state, to be resumed on future ticks.

### 3. External Function Bindings (`sithCogFunction.c`)
This file maps VM string calls (e.g., `GetSenderRef`) to C function pointers. 
- **Lookup Overhead:** On PC, this uses string hashing and lookups.
- **N64 Optimization:** For the N64, the AOT compilation pipeline (BCOG) must replace string lookups with direct integer IDs or array indices to the C function table to save RAM and CPU cycles.

## N64 Porting Strategy
- **Gut the Parser:** Remove `lex.yy.c`, `sithCogParse.c`, and `byacc`/`flex` dependencies from the N64 build.
- **Implement Pre-compiled Bytecode (BCOG):** The data pipeline must output raw opcode streams and resolved symbol IDs, eliminating all `malloc` calls currently used during the parsing phase.