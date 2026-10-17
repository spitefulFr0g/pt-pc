# Triage labels

Canonical roles map to these GitHub label names:

| Role | Label | Meaning |
| --- | --- | --- |
| needs-triage | `needs-triage` | Maintainer needs to evaluate the issue. |
| needs-info | `needs-info` | Waiting on the reporter for information. |
| ready-for-agent | `ready-for-agent` | Fully specified and ready for agent implementation. |
| ready-for-human | `ready-for-human` | Requires human implementation. |
| wontfix | `wontfix` | Will not be actioned. |

When a skill names a triage role, apply its mapped label. A published implementation spec uses `ready-for-agent`. Wayfinder maps and decision tickets use their separate `wayfinder:` labels.
