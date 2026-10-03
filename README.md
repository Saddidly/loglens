# LogLens

A dependency-free C11 command-line tool for finding recurring failures in local
service logs without loading entire files into memory.

LogLens reads one or more files or stdin, counts severity levels, filters relevant
lines, and groups recurring messages by a normalized template. JSON output and
explicit exit statuses make it usable in incident triage scripts and CI jobs.

## Example

```sh
loglens --min-level WARN --top 10 examples/service.log
loglens --json --contains upstream examples/service.log > summary.json
cat service.log | loglens --json --fail-on-error
```

Two messages such as `ERROR request 812 upstream timeout` and
`ERROR request 913 upstream timeout` share the template
`error request # upstream timeout`. Repeated whitespace is collapsed and ASCII
letters are lowercased. Template grouping is a triage heuristic, not a semantic
parser. Inspect the original log before drawing conclusions.

## Build and test

Requires a C11 compiler, CMake 3.20+, and Python 3 for behavioral tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is `build/loglens` with Unix Makefiles/Ninja, or
`build/Release/loglens.exe` with Visual Studio. For a direct build:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/main.c src/analysis.c src/report.c -o loglens
python3 tests/test_cli.py ./loglens
```

Zig users can replace `cc` with `zig cc`; Windows executable suffixes apply.
For a direct MinGW/Zig Windows build, add `-municode` for Unicode CLI arguments.
No compiler downloads or administrator privileges are needed by the application.

## Options and behavior

| Option | Purpose |
| --- | --- |
| `--json` | Versioned machine-readable summary |
| `--min-level WARN` | Retain WARN, ERROR, and FATAL templates |
| `--contains TEXT` | Retain templates from lines containing an exact substring |
| `--top N` | Print at most N templates, ordered by frequency |
| `--max-patterns N` | Bound distinct-template memory, default 5,000 |
| `--max-line N` | Maximum line bytes, default 1 MiB, maximum 16 MiB |
| `--fail-on-error` | Return status 3 if any classified input line is ERROR/FATAL |
| `--` | End options, allowing filenames starting with a hyphen |

With no filenames, stdin is read. `-` reads stdin among file arguments; it may be
used once. Files are processed in argument order. CRLF and a final line without
a newline are accepted. Lines containing NUL or exceeding the configured bound
are skipped and counted; analysis resumes at the next newline.

The **first whole severity token** anywhere on a line is used. Tokens are
TRACE, DEBUG, INFO, WARN/WARNING, ERROR, FATAL/CRITICAL, ignoring ASCII case.
An unrecognized line is UNKNOWN. This works with common bracketed and textual
logs, but message words can look like severity tokens; no JSON schema or custom
log-format parser is promised.

Severity totals describe all accepted input lines. Filters affect `matched` and
templates, not severity totals or `--fail-on-error`. UNKNOWN is included unless
`--min-level` is set. Patterns are limited to 511 output bytes; `truncated: true`
marks a prefix that can merge messages differing later in the line.

## Report and exit status

JSON includes `schema_version`, `lines`, `matched`, `levels`, `patterns`,
`skipped_long`, `skipped_binary`, `untracked_patterns`, and `report_complete`.
`untracked_patterns` counts matching **lines** whose new template could not be
stored after capacity was reached. Already stored templates keep accumulating.
`report_complete` is false when input lines/templates are omitted or report
ordering could not be allocated. `--top` intentionally limits displayed patterns.

| Status | Meaning |
| --- | --- |
| 0 | Completed within configured bounds |
| 1 | Partial input/report, unreadable file, capacity reached, or output failure |
| 2 | Invalid configuration or initialization failure |
| 3 | Complete analysis found ERROR/FATAL with `--fail-on-error` |

Partial analysis takes precedence over status 3. Review stderr and counters.

## Resource and encoding limits

Memory is bounded by the maximum line, template capacity, hash index, and report
ordering. Files are streamed using buffered standard I/O. Digit runs are replaced
with `#`; decimals, IDs, and timestamps can therefore collapse together.
UTF-8 is preserved in JSON when valid; invalid bytes are escaped as U+00xx for a
valid report. The CLI does not redact secrets from log text. It does not follow
rotating files or compute time-window rates.

See [architecture](docs/architecture.md), [contributing](CONTRIBUTING.md), and
[security](SECURITY.md). Licensed under [MIT](LICENSE).
