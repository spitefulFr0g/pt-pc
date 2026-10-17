# Domain docs

This is a single-context project: one root `GLOSSARY.md` and repository-wide decisions under `docs/adr/`.

## Before exploring a feature

Read the root `GLOSSARY.md` and ADRs relevant to the feature under `docs/adr/`. If the repository later adopts a root `GLOSSARY-MAP.md`, follow it to the relevant context glossaries and decisions.

If any of these documents are absent, proceed silently. Create terms and decisions lazily through the domain-modeling workflow when a discussion resolves them.

## Vocabulary and decisions

Use glossary terms in specifications, issue titles, hypotheses, test names, and implementation discussions. If an important concept lacks a definition, first check whether an existing term covers it; record a real gap for domain modeling.

Surface an explicit conflict with a relevant ADR before proposing to depart from it, naming the decision and explaining why it should be reopened.
