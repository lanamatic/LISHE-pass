# LISHE-pass
Project for Compiler Construction course:
- Loop-Invariant Store Hoisting/Elimination (LISHE) — a custom LLVM optimization pass.This pass detects stores inside loops whose target pointer and value do not change across iterations and safely moves or removes them:
    -  Store hoisting: moves an invariant store out of the loop.
    - Redundant store elimination: removes repeated identical stores in the same loop body.

- The repository implements a stand-alone LLVM pass and a small test suite. t demonstrates:

    - Custom LLVM pass development using the new pass manager.
    -  Fine-grained reasoning about loop invariants, aliasing, and dominance.
    - Practical code-generation improvements (fewer memory writes, shorter loops).
- Example transformation:
``` c
// Before
for (int i=0; i<n; ++i) {
    *p = v;
    *p = v;   // redundant
}

// After
*p = v;       // hoisted & deduplicated
for (int i=0; i<n; ++i) { /* empty */ }

```

# Requirements
- LLVM/Clang ≥ 15
- CMake / Ninja
- Standard Unix shell utilities (```bash```, ```diff```, ```make/ninja```)
### Installations
MacOS (Apple Silicon or Intel) - install LLVM via Homebrew:
```bash
brew install llvm cmake ninja
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
exec zsh  
# sanity check:
which opt  # /opt/homebrew/opt/llvm/bin/opt
which clang  # /opt/homebrew/opt/llvm/bin/clang
```
Linux (example: Ubuntu 22.04+):
```bash
sudo apt update
sudo apt install llvm-15 llvm-15-dev clang-15 cmake ninja-build git diffutils
# use the matching LLVM toolchain:
export CC=clang-15
export CXX=clang++-15
export PATH="/usr/lib/llvm-15/bin:$PATH"
# sanity check:
llvm-config --version # should be ≥15
```
# Building the Pass
### Clone and build:
```bash
git clone https://github.com/yourname/LISHE-pass.git
cd LISHE-pass
mkdir -p build && cd build
```
```bash
# MacOS
cmake .. -G Ninja \
  -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -j"$(sysctl -n hw.ncpu)"
```
```bash
# Linux
cmake .. -G Ninja \
  -DLLVM_DIR="$(llvm-config --cmakedir)" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -j"$(nproc)"
cd ..
```
Output: ```build/LISHEPass.dylib``` (macOS) or ```build/LISHEPass.so``` (Linux)
### Running the tests
####  Runnig all the tests:
```bash
chmod +x run_tests.sh
./run_tests.sh
```
This generates, for each tests/*.c file:
- *_before.ll (unoptimized IR)
- *_after.ll (after mem2reg,lishe)

Key tests in directory tests/ include:

- ```01_basic_hoist``` - Simple hoist of a single store
- ```02_guarded_loop``` - Intentional demo: shows a hoist after a read (illustrates a bug)
- ```03_alias_write_blocker``` - Hoist blocked due to possible aliasing
- ```04_pointer_varies``` -Pointer depends on loop index (base[i]=v) → not loop-invariant
- ```05_volatile_store``` - Atomic/volatile-like store must not move
- ```06_redundant_same_store``` - Duplicate stores → deduplicate + hoist

- Inspect results e.g.:
``` bash
diff -u tests/01_basic_hoist.ll tests/01_basic_hoist.ll
```

#### Using the Pass Manually:
``` bash
# Compile to truly unoptimized IR
cd build
clang -S -emit-llvm -O0 \
  -Xclang -disable-llvm-passes \
  -Xclang -disable-O0-optnone \
  -o input.ll ../path/to/your_input.c

# Run LISHE (and mem2reg)
opt -load-pass-plugin build/LISHEPass.$([ "$(uname)" = "Darwin" ] && echo dylib || echo so) \
-passes="mem2reg,lishe" -S input.ll -o output.ll
```
Why ```mem2reg```? 
LISHE logic checks SSA-level loop-invariance. At ```-O0``` Clang gives you IR with allocas + loads inside the loop, so the store’s operands don’t look invariant yet. ```mem2reg``` rewrites that IR into SSA, exposing the invariants so your checks pass.

# Documentation
Screenshots of representative diffs for some tests are included in [PresentationSerbian.pdf](PresentationSerbian.pdf)

# Authors
Jovana Urošević 189/2021 \
Lana Matić 143/2021