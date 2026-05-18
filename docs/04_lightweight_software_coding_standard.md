<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Lightweight Software Coding Standard

## 1. Purpose

This document defines the lightweight coding rules used for the recorder-core software. The intent is to support structured review and maintainability while preserving the existing prototype code style as much as practical.

This document is not a formal DO-178C coding standard. It is a project coding standard inspired by safety-oriented development practices and intended to support authority review of the software development approach.

## 2. Scope

This standard applies to the first controlled recorder-core scope:

- `state_task` and high-level recorder state handling;
- `sd_task` and recording-related SD state handling;
- acceleration data acquisition;
- ring-buffer handling;
- record formatting;
- recording-related SD storage functions;
- required underlying drivers and services used by the recorder core.

The Web and UI implementations remain support functionality and are outside the first controlled operational scope. When Web/UI code is modified, these rules should still be followed where practical to keep the project consistent.

## 3. General Principles

The code should favor:

- simple state machines;
- explicit ownership of task state;
- bounded buffers;
- explicit return-value checking;
- minimal public APIs;
- module-local helpers and state where possible;
- comments that explain purpose, inputs, outputs, and non-obvious constraints.

The objective is readability and reviewability rather than strict compliance with an external coding standard.

## 4. Naming Rules

- Public functions should use a module prefix, such as `sd_`, `sd_files_`, `state_task_`, `ui_`, or `web_`.
- Private file-scope helper functions should use a trailing underscore when practical, for example `helper_()`.
- File-scope static variables should use the `s_` prefix.
- Constants and preprocessor configuration values should use uppercase names.
- Project types should use the `_t` suffix where consistent with existing code.
- Existing names may be retained when renaming would create unnecessary churn.

## 5. File and Header Organization

- Headers should expose only declarations required by other modules.
- Source files should keep private helpers and private state `static`.
- Headers should avoid including large implementation headers unless their public declarations require those types.
- Source files should include the headers they directly depend on rather than relying on transitive includes.
- Unused source files, declarations, functions, and includes should be removed when identified.


### 5.1 Source File Organization Convention

Where practical, implementation files should be organized in this order:

1. includes;
2. private module state;
3. private helper functions;
4. task/state-machine implementation;
5. public API functions.

Public API functions are preferably grouped near the end of the `.cpp` file. This keeps the implementation flow readable while still making the public entry points easy to find. Exceptions are acceptable when moving a function would require unnecessary forward declarations or reduce readability.

## 6. Task Ownership Rules

- Each task owns its own task-level state machine and task-local state.
- Other modules shall access task state only through explicit APIs.
- `state_task` owns the high-level recorder state and user-visible error-manager updates.
- `sd_task` owns recording-related SD state and recording file operations.
- Raw SD storage operations used by recording shall execute from SD-task context.
- UI code shall render state and messages but shall not own recorder-core decisions.
- Web code is support functionality and shall not directly perform raw SD storage access.

## 7. Error and Message Handling Rules

- User-visible recorder-core errors shall be represented by `error_code_t`.
- `state_task` shall be the owner of `error_manager` raise/clear/update decisions.
- Other tasks shall expose local status or error conditions through narrow APIs.
- `error_manager` shall provide the mapping from active error code to user-visible message and clearability behavior.
- Normal recorder-core errors should not force a reboot when user recovery is possible.
- Fatal initialization failures, such as failure to create a required task, shall print a serial message, wait briefly, and reboot.

## 8. Return-Value Rules

- Functions returning `bool` shall use `true` for success and `false` for failure.
- Functions returning `error_code_t` shall use `ERR_NONE` for success.
- Callers shall check return values for operations that can fail.
- A return value shall not be ignored unless the code explicitly documents or marks that the result is intentionally unused.

## 9. Memory and Buffer Rules

- Dynamic allocation should be avoided in recorder-core paths unless there is a clear reason and bounded lifetime.
- Fixed-size buffers should be sized from configuration constants when a project limit exists.
- String formatting should use bounded functions such as `snprintf`.
- Path, filename, file-count, and file-list buffer sizes should derive from the project configuration limits.
- Code should avoid unbounded string copying or formatting.

## 10. Control-Flow Rules

- State-machine behavior should be explicit and easy to review.
- Deep helper chains should be avoided unless they reduce duplication or centralize an important rule.
- Helpers should represent a clear concept, such as state ownership, error classification, path normalization, or repeated validation.
- Disabled legacy code should not remain in source files as `#if 0` blocks. Historical code belongs in version control history, not in the active baseline.

## 11. Comment Rules

- Each file should have a brief purpose comment.
- Each function should have a brief comment describing purpose, inputs, and return value.
- Comments should explain intent, assumptions, ownership, and non-obvious constraints.
- Comments should not contain formal requirement identifiers until a formal traceability scheme is introduced.
- Comments should not simply restate obvious code.

## 12. Review Checklist

For each controlled baseline review, reviewers should check that:

- public interfaces are documented;
- module ownership is clear;
- private helpers and state are not exposed unnecessarily;
- task ownership rules are respected;
- return values are checked;
- recorder-core buffers use bounded sizes;
- error handling follows the centralized ownership model;
- Web/UI support code is not treated as controlled recorder-core behavior unless explicitly reclassified;
- comments are present and useful without embedding premature requirement identifiers.

## 13. Deviation Handling

A deviation from this standard is acceptable when it preserves working prototype behavior or avoids unnecessary churn. The reason should be documented in a code comment, review note, or change record when the deviation affects recorder-core behavior.

## Implementation Documentation Consistency

State-machine behavior that affects recorder operation shall be reflected in the documentation set when changed. In particular, changes to recording start, recording stop, SD error handling, low-space handling, and fatal/recoverable error behavior should update the behavior review, requirements outline, and architecture documents.

Source comments should describe the implemented behavior without introducing formal requirement identifiers until the project decides to maintain formal traceability.
