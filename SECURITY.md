# Security Policy

## Supported Versions

This project is a single-maintainer educational/portfolio project. Security fixes are applied to the `main` branch only.

| Branch | Supported |
|--------|-----------|
| `main` | ✅ Yes |
| Older tags | ❌ No |

---

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

If you discover a security issue, report it privately by:

1. Navigating to the repository's **Security** tab on GitHub.
2. Clicking **"Report a vulnerability"** to open a private advisory draft.

Alternatively, contact the maintainer directly via the email listed on their GitHub profile.

### What to include

- A clear description of the vulnerability
- Steps to reproduce (minimal proof-of-concept if applicable)
- The potential impact (e.g., buffer overflow, arbitrary file write, denial of service)
- Your suggested fix, if you have one

### What to expect

- An acknowledgement within **72 hours**
- A status update (confirmed / not reproducible / out of scope) within **7 days**
- If confirmed, a fix will be developed and a patched release published. You will be credited in the release notes unless you prefer to remain anonymous.

---

## Scope

This is a POSIX terminal editor. The attack surface is intentionally narrow:

| Area | Notes |
|------|-------|
| File I/O | Reads and writes only the file passed on the command line |
| Memory safety | Continuously validated with AddressSanitizer + UBSan (`make debug`) |
| Regex engine | Thompson NFA — no backtracking; O(pattern × text) worst-case guaranteed |
| Autosave thread | Single shared mutex; snapshot copied before any disk write |
| External dependencies | None beyond C++17 stdlib + POSIX |

Reports that fall outside this scope (e.g., issues in the build toolchain or the underlying OS) are generally out of scope.

---

## Security Best Practices for Users

- Always build from a tagged release or a commit you have reviewed.
- Run `make debug` to validate memory safety before deploying a custom build.
- Do not open untrusted files with elevated privileges.
