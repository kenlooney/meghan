# The Meghan Compiler
### Build Status
The compiler currently builds and runs on:
- Windows
- Ubuntu

### Running the tests
On Windows
```bash
cmake --build out/windows --config Debug
ctest --test-dir out/windows -C Debug --output-on-failure
```
On Linux
```bash
cmake --build out/linux
ctest --test-dir out/linux --output-on-failure
```