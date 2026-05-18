<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Documentation Map

## 1. Purpose

This document is the entry point for the recorder software documentation set.

It only explains:

- the purpose of each document;
- the intended reading order;
- which document owns each type of information;
- the current document numbering scheme.

Scope, requirements, architecture decisions, state-machine behavior, coding rules, validation strategy, and recording-format details are owned by the documents listed below, not by this map.

## 2. Document Set

| Document | Purpose | Primary owner of |
|---|---|---|
| `00_documentation_map.md` | Explains the documentation structure and reading order | documentation organization only |
| `01_recorder_requirements.md` | Captures operational requirements, configuration values, implementation allocation, message/error list, validation evidence, and recording block definitions | requirements, traceability, and block definitions |
| `02_recorder_architecture.md` | Captures software scope, operating concept, module ownership, data/control flow, architecture rationale, and support-vs-controlled behavior boundaries | architecture and operational concept |
| `03_state_machine_behavior_review.md` | Captures code-grounded behavior of `state_task`, `sd_task`, and calibration session servicing | implemented state behavior |
| `04_lightweight_software_coding_standard.md` | Captures lightweight coding and maintainability rules for the prototype recorder-core software | coding rules |
| `05_lightweight_validation_strategy.md` | Captures practical validation strategy and validation case candidates | validation planning and evidence approach |

## 3. Recommended Reading Order

For requirements and design review:

1. `01_recorder_requirements.md`
2. `02_recorder_architecture.md`
3. `03_state_machine_behavior_review.md`
4. `04_lightweight_software_coding_standard.md`
5. `05_lightweight_validation_strategy.md`

For file-format or post-processing work:

1. `01_recorder_requirements.md`, Section 8
2. `05_lightweight_validation_strategy.md`

For code review:

1. `01_recorder_requirements.md`
2. `02_recorder_architecture.md`
3. `03_state_machine_behavior_review.md`
4. `04_lightweight_software_coding_standard.md`

## 4. Information Ownership Rules

To avoid duplication and drift:

| Information type | Owning document |
|---|---|
| Operational requirements | `01_recorder_requirements.md` |
| Configuration values used by requirements | `01_recorder_requirements.md` |
| Requirement-to-code mapping | `01_recorder_requirements.md` |
| Message/error triggering logic | `01_recorder_requirements.md` |
| Binary recording block layouts | `01_recorder_requirements.md` |
| Software scope and operational concept | `02_recorder_architecture.md` |
| Module ownership and architecture rationale | `02_recorder_architecture.md` |
| Detailed state-machine behavior | `03_state_machine_behavior_review.md` |
| Coding rules | `04_lightweight_software_coding_standard.md` |
| Validation approach and validation case candidates | `05_lightweight_validation_strategy.md` |

## 5. Maintenance Rule

When a behavior changes:

1. update the requirement in `01_recorder_requirements.md`;
2. update architecture allocation in `02_recorder_architecture.md` if ownership or data flow changes;
3. update `03_state_machine_behavior_review.md` if state behavior changes;
4. update recording block definitions in `01_recorder_requirements.md` if recorded-file layout changes;
5. update `05_lightweight_validation_strategy.md` if validation evidence or procedure changes.
