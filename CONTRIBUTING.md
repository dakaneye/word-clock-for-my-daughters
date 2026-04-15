# Contributing

Personal multi-year project. External PRs are welcome but may move slowly.

## Building

Firmware is a PlatformIO project under `firmware/` (scaffolded in a follow-up commit). Once present:

```bash
cd firmware
pio run -e emory    # build Emory's firmware
pio run -e nora     # build Nora's firmware
pio test -e native  # run host-side unit tests
```

Hardware (KiCad) and enclosure (CAD + SVG) sources arrive in later phases — see `docs/superpowers/plans/`.

## Pull requests

- Branch prefix: `dakaneye/` (e.g., `dakaneye/add-holiday-palette`)
- Commit format: conventional commits — `type(scope): subject` in imperative mood, no period, max 50 chars
- Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`
- One logical change per PR. Fill in the PR template.
- CI must be green before merge.

## Conventions

Read [`conventions.md`](conventions.md) before implementing anything non-trivial. It covers project layout, firmware module rules, testing expectations, and hardware conventions.

## Questions

Open an issue, or email samjdacanay@gmail.com.
