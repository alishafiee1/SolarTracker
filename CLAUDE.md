<!-- rodo:start -->
This repo uses the RODO method (kit 2.2.1). Before any work: read `.agents/skills/rodo/SKILL.md`,
run `rodo sync`, then `rodo resume`. At the end of every session run `rodo sync`.
RODO projects: see `rodo/*/project.json`. Never write secrets into rodo files. Never create boards.
<!-- rodo:end -->

## Graphify (code map)

Graphify is installed on this machine. Build or refresh the map with `graphify update .`
(writes `graphify-out/`, not committed). Use `graphify query "<question>"` to find code and
`graphify affected "<symbol>"` when filling a card's `## Documentation Impact`.
Run `graphify update .` again after code changes.
