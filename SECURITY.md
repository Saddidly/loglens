# Security

LogLens reads local files and emits a report. Run it with ordinary user privileges
on directories or inputs you trust. Reports can disclose private paths or log data;
review them before sharing.

Before opening a public issue, remove credentials and personal data from examples.
For a suspected vulnerability, use GitHub private vulnerability reporting once
the repository owner enables it. No private reporting endpoint is configured in
this local project yet.

Inputs have explicit resource and trust boundaries documented in README.md. This
software does not provide a sandbox, a filesystem snapshot, or a security audit.
