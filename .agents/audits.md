# Audits

Store repository audits in `.agents/audits/`.

Each audit should be written as its own file inside that directory.

Required metadata for every audit:
- production date
- commit hash for the revision that was audited

Recommended filename pattern:
- `YYYY-MM-DD-<commit-hash>-<topic>.md`

This keeps audit history separate from the standing agent docs while preserving when the audit was produced and which code revision it covered.
