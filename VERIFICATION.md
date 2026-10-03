# Verification

Local evidence recorded on 2026-10-03.

## Passed

Strict C11 compilation and CMake build/CTest passed. The CLI suite passed 14 tests, including Unicode paths/content, binary data, input limits, filtering, and exit codes.

## Environment and limits

Windows Clang 19 through Zig 0.14.1; Python 3.12.14. Linux/MSVC CI is configured but has not run on GitHub.

The checked-in CI workflow is ready to run when published. It is configuration,
not evidence of a hosted pass. Re-run README commands after changing dependencies
or moving to another platform. Screenshots, where included, use synthetic data.
