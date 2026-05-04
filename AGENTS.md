# Agentic coding guidelines

The main git repository for this project is at https://github.com/ned14/llfio.

1. All source, header and test files MUST be kept compatible with the 2017
ISO C++ standard. Where using a latest ISO C++ standard feature would make
sense, try to use the existing preprocessor macro infrastructure where
possible. Otherwise ask for direction on what to do.
2. Run `clang-format` on every changed header and source file. Do NOT run
`clang-format` on cmake files.
3. When building and testing, extract what to do for the current platform
from `.github/workflows/unittests_*.yml`.
4. Ignore everything in the `attic` and `single-header` directories.
