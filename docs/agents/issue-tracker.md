# Issue tracker: GitHub

Issues, implementation specs, tickets, and new wayfinder maps live in GitHub Issues for `spitefulFr0g/pt-pc`. Use the authenticated `gh` CLI with an explicit `--repo spitefulFr0g/pt-pc` when the command accepts it.

## Issue operations

- Publish an issue or spec with `gh issue create --repo spitefulFr0g/pt-pc --title '...' --body-file <file>`. Use a UTF-8 file with actual newlines for multiline content.
- Read with `gh issue view <number> --repo spitefulFr0g/pt-pc --json number,title,body,labels,comments`.
- List with `gh issue list --repo spitefulFr0g/pt-pc --state open --json number,title,body,labels`, adding label/state filters as needed. Fetch comments when they inform the work.
- Comment with `gh issue comment <number> --repo spitefulFr0g/pt-pc --body-file <file>`.
- Apply or remove labels with `gh issue edit <number> --repo spitefulFr0g/pt-pc --add-label <label>` or `--remove-label <label>`.
- Close with `gh issue close <number> --repo spitefulFr0g/pt-pc`, adding a resolution comment when required by the active skill.
- When a skill says to publish to the issue tracker, create a GitHub issue. When it says to fetch a ticket, read the issue and relevant comments.

## Pull requests as a triage surface

**PRs as a request surface: no.**

## Wayfinding operations

- A map is one issue labeled `wayfinder:map`; its decision tickets are child issues labeled `wayfinder:research`, `wayfinder:prototype`, `wayfinder:grilling`, or `wayfinder:task`. Maps and decision tickets use only wayfinder labels, rather than implementation triage labels.
- Create each child issue, then attach it with GitHub's native sub-issue API: `gh api --method POST repos/spitefulFr0g/pt-pc/issues/<parent>/sub_issues -F sub_issue_id=<child-database-id>`. Obtain the database ID with `gh api repos/spitefulFr0g/pt-pc/issues/<child> --jq .id`. If sub-issues are unavailable, use a task list on the map and `Part of #<parent>` on the child.
- Express blocking with native dependencies: `gh api --method POST repos/spitefulFr0g/pt-pc/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-database-id>`. If dependencies are unavailable, use a `Blocked by: #<number>` line and check referenced issues' states.
- Find the frontier among the map's open children: no open blockers and no assignee, in map order. Native `issue_dependencies_summary.blocked_by` counts open blockers. Fetch sub-issues with pagination where required.
- Claim by assigning the selected child to the driving developer before working. Resolve by recording the answer, closing the child, and adding a named link with a short decision summary to the map.
- Existing local planning artifacts remain source material. This setup does not migrate or recreate the resolved local launcher map or its decision tickets.
