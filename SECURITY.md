# Security Policy

## Public Repository Rules

Do not commit secrets or private machine data to this repository.

Never commit:

- Wi-Fi SSIDs or passwords
- API keys, tokens, private keys, or certificates
- `.env` files or local credential headers
- private serial-port logs that expose a participant machine, account, or network
- closed-source app, server, or desktop package source code

The repository policy check scans for common secret patterns and private path markers, but contributors are still responsible for reviewing their changes before opening a pull request.

## Reporting Sensitive Issues

If the issue can be described without exposing a secret, open a GitHub issue and mark it as security-sensitive in the title or first paragraph.

If the report requires sharing credentials, private logs, or unpublished exploit details, do not paste them into a public issue. Contact the repository maintainers through the organization account or an existing private event support channel, then share only the minimum reproduction data needed.

## Supported Scope

Security reports for this public repository may cover:

- embedded firmware code in `firmware/`
- public flashing and release tools in `tools/`
- public protocol documentation in `firmware/` and `docs/`
- hardware publication files when they affect safe reproduction

Closed-source app, server, and desktop packages are distributed separately as release assets when available.
