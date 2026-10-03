# Contributing to LogLens

Start with a reproducible problem or a concrete use case. Small changes that
preserve the documented CLI and report schema are easier to review.

1. Configure a debug build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.
2. Build: `cmake --build build --config Debug`.
3. Run behavioral checks: `ctest --test-dir build -C Debug --output-on-failure`.
4. Add a fixture that exercises the changed behavior, including failure cases.
5. Document changes to arguments, report fields, exit codes, or resource limits.

Use the existing module boundaries. Avoid dependencies that only replace a few
clear lines of standard-library code. Never add filesystem mutation to a read-only
analysis command without a separate design discussion. Do not commit private logs,
real directory reports, credentials, compiler binaries, or generated builds.

CI is configured for Linux and Windows. A configured workflow does not establish
that those hosted jobs have run; verify the checks after creating your repository.
