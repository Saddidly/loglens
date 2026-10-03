# Verification

Local evidence recorded on 2026-10-03.

## Passed

Strict C11 compilation and CMake build/CTest passed. The CLI suite passed 14 tests, including Unicode paths/content, binary data, input limits, filtering, and exit codes.

## Environment and limits

Windows Clang 19 through Zig 0.14.1; Python 3.12.14. Linux GCC and Windows MSVC hosted checks also passed; see the linked evidence below.

## Hosted evidence

[GitHub Actions run](https://github.com/Saddidly/loglens/actions/runs/37111250099) passed on 2026-10-03 for code revision `aaf7db0d92282d12b5036c975e2932ee3273e4ca`.

Ubuntu GCC and Windows MSVC; CMake build and CLI tests.

These checks cover the named environments and cases, not every possible input or platform. Re-run README commands after changing dependencies or moving to another platform. Screenshots and acceptance data are synthetic.

