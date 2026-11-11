### Summary

What does this PR do? Why are these changes necessary?

### Spec traceability

Feature and spec work must link artifacts. Trivial docs/chores that do not change song behavior, UI contracts, audio budgets, or install/recovery may mark N/A with a one-line justification.

- Issue:
- Spec (`spec.md`):
- Plan (`plan.md`):
- Tasks (`tasks.md`):

### Constitution Check

- [ ] No invented `.bax` syntax or undocumented BeatBax language behavior
- [ ] Song model / expansion / timing impact called out (or N/A)
- [ ] Chip voice isolation preserved (or N/A)
- [ ] Audio callback rules respected (or N/A for host-only)
- [ ] UI remains graphical; no on-panel source editor added (or N/A)
- [ ] Licensing boundary respected (host Apache; Felucca only in firmware)

### Checklist

- [ ] My changes add required tests and they pass for this phase.
- [ ] I ran the test command(s) below locally.
- [ ] I updated README, `docs/`, or `specs/` if required.
- [ ] Flashing / recovery docs updated if install steps changed (or N/A).
- [ ] This PR follows repository coding style and guidelines.

### Tests run

```text
# e.g. host unit tests, beatbax verify on fixtures, hardware checklist path
```

### Notes for reviewers

Anything specific you'd like reviewers to check or be aware of.
