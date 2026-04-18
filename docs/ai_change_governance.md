# AI Change Governance

## Rules

- AI never mutates active CAD branch directly.
- Every AI change is committed on a local proposal branch.
- Checker must pass before merge is enabled.
- User approval is required to merge.
- Rejected proposals must not affect active branch state.
