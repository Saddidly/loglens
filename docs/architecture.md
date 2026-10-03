# Design and tradeoffs

`main.c` owns arguments, input streams, diagnostics, and exit precedence.
`analysis.c` owns line framing, classification, normalization, bounded aggregation,
and resource cleanup. `report.c` owns deterministic ordering and serialization.
The public C interface permits embedding the analyzer without the CLI.

The analyzer retains one bounded line and a fixed-capacity open-addressed index.
Slots store a one-based index into owned patterns; zero denotes an empty slot.
The index has more than twice the pattern capacity, so probing always has an
empty slot even when new patterns are refused. Hash collisions are resolved by
checking severity, truncation state, and exact normalized text.

Line framing consumes oversized lines fully before resuming. Binary lines are
excluded rather than truncating at NUL and misclassifying the remainder. Every
allocation has an owner; destruction also handles partial initialization.
Allocation or read failure leaves consumed counters available in the report.

Templates intentionally discard detail. This is useful for finding frequent
incident families, but cannot prove two lines have the same cause. Grouping is
not suitable for compliance counts or lossless log transformation. Capacity is
explicit, with omitted lines counted and a partial exit status, rather than an
unbounded map that can exhaust memory on high-cardinality logs.

The test suite invokes the built executable against real streams and files,
covering severity boundaries, filtering, aggregation, Unicode, arbitrary bytes,
oversized lines, multiple inputs, exit status, and capacity recovery. No speed
claims or large-scale benchmarks have been established for this initial version.
