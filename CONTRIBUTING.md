# Contributing

Contributions are welcome: bug reports with evidence ([REPORTING.md](REPORTING.md)), tests of the untested areas in
[docs/STATUS.md](docs/STATUS.md), and decomp/tooling changes (see [AGENTS.md](AGENTS.md) for the matching workflow).

## Rules for every change
- No game data: no disc contents, extracted files, BIOS images, saves, generated C or built executables. This repository
  must stay free of them (see [NOTICE](NOTICE)).
- Run the checks that need no game data before you push:
  `python -I tests/slz/run_tests.py`, `python -I scripts/rules.py` and `python -I scripts/repo_guard.py`.
  CI runs the same three (`.github/workflows/guard.yml`). `rules.py` currently reports one known FAIL
  (`src/func_8003B5DC.c`, rule R4) and many WARNs; do not add new FAILs.
- Matched functions: keep to the rule checker and the evidence conventions in AGENTS.md.
- Keep claims honest: say what you tested, with the log line or screenshot; say what you did not.

## AI-assisted contributions
This project is itself built largely with an AI assistant, so AI help is fine. If you use it:
- **Disclose it** in the pull request ("AI-assisted: tool/model, what it did") and add a `Co-Authored-By` trailer
  to commits where an AI wrote a substantial part.
- **You are responsible** for every line: read it, run it, and be able to explain it. Unreviewed AI output is not accepted.
- **Never paste game data** (disassembly dumps of long ranges, extracted text, disc bytes, BIOS code) into an AI tool or
  into the repository beyond the short snippets needed to explain a function.
- **Run the gate and guards** listed above, and say so in the PR.
- **Protect your identity**: commit with a GitHub noreply address (`<id>+<name>@users.noreply.github.com`), and make sure
  logs, notes and code contain no home paths (write `<home>`), real e-mail addresses or machine names.
  `scripts/repo_guard.py` checks the common cases.

## Licence
By contributing you agree your original contribution is licensed under the MIT licence in [LICENSE](LICENSE). Third-party
components keep their own licences ([THIRD_PARTY.md](THIRD_PARTY.md)).
