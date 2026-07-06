# zssh Design

Date: 2026-07-03
Status: accepted design baseline

## 1. Overview

`zssh` is a customized remote command execution tool with an SSH-like workflow. It targets operators and automation users who need both an ergonomic interactive terminal experience and a machine-readable execution surface.

This design freezes the product shape before implementation. It captures the approved architecture, command surface, configuration model, runtime responsibilities, and platform boundaries that later implementation work must respect.

`zssh` is also the intended reusable SSH runtime base for future derived CLIs. In particular, a future `zazaki_scp` command-line tool should be implemented as a thin CLI facade over the same copy/session/protocol stack rather than as a second SSH or file-transfer implementation.

## 2. Goals

- Provide a single CLI tool for interactive connection, remote execution, file transfer, key generation, proxying, and configuration management.
- Keep protocol logic inside `libssh` while presenting a higher-level product surface tailored to `zssh` workflows.
- Support both human-first terminal interaction and AI-friendly machine output through a dedicated `--json` mode.
- Run on `Linux` and `Windows 10 1909+` from one codebase.
- Make session lifecycle, backpressure handling, and terminal rendering explicit design concerns rather than incidental implementation details.
- Make copy/transfer behavior reusable so a future `zazaki_scp` frontend can share the same runtime, authentication, error model, and transport behavior.

## 3. Non-goals

- Do not shell out to a third-party SSH client.
- Do not treat platform-specific eventing or terminal APIs as acceptable cross-layer shortcuts.
- Do not defer session management, backpressure, or structured output to a later "hardening" phase.
- Do not build a second SSH or file-transfer stack for `zazaki_scp`.
- Do not promise full `rsync` parity in the first-phase copy implementation.

## 4. Technology Baseline

- Language: `C++`
- Protocol library: `libssh`
- Build system: `CMake`
- Target platforms: `Linux`, `Windows 10 1909+`
- Planned platform primitives: `epoll` and PTY on Linux, `IOCP` and `ConPTY` on Windows

These choices are frozen for the design and implementation planning phases.

## 5. User-facing Surface

### 5.1 Commands

The CLI command family is frozen as:

- `connect`
- `exec`
- `config`
- `keygen`
- `proxy`
- `copy`

These commands form the primary navigation model of the tool. Later implementation may refine flags and exact syntax, but it must not replace this verb set with a different command taxonomy.

### 5.2 Execution Modes

`zssh` serves two primary usage modes:

- Human-oriented interactive usage, centered on terminal rendering and session continuity.
- Machine-oriented execution, centered on stable structured output via `--json`.

The design assumption is that both modes are first-class and must share the same underlying session and protocol layers.

### 5.3 Copy Surface

`copy` is a first-class command, but its implementation is not owned by the `zssh` CLI layer alone.

The design rules are:

- `zssh copy` is one frontend over a reusable copy runtime.
- a future `zazaki_scp` should map `scp`-style arguments into the same underlying copy runtime instead of re-implementing transfer logic.
- first-phase copy behavior is `scp`-like rather than full `rsync`-like synchronization.
- copy operations must share the same profile/authentication/session/error/JSON conventions as the rest of `zssh`.

### 5.4 Configuration Format

Persistent configuration uses `YAML`.

The configuration surface is responsible for at least:

- connection profiles
- authentication-related settings
- default command behavior
- output preferences
- theme selection
- session-related policy knobs

CLI flags may override config values at runtime, but the durable source of user intent is the YAML configuration surface.

## 6. Architecture

The approved system architecture is:

`CLI -> Session Manager -> Protocol Adapter(libssh) -> Terminal Renderer -> Platform Abstraction`

Each layer has a single primary responsibility and a narrow downstream contract.

The arrow notation describes responsibility flow, not an exclusive one-hop call graph. `Platform Abstraction` is a shared lower layer that may be consumed by the protocol and rendering layers through explicit interfaces.

### 6.1 CLI

Responsibilities:

- parse user input
- resolve command verb and options
- load and merge YAML configuration with command-line overrides
- select human-readable output or `--json` output mode
- hand a normalized request to the session manager

The CLI does not own SSH protocol behavior, terminal state, or platform event loops.

For copy-specific workflows, the CLI is responsible only for argument normalization and request construction. It must not embed transport behavior that would prevent reuse by `zazaki_scp`.

### 6.2 Session Manager

Responsibilities:

- own session lifecycle
- map CLI intent to runtime operations
- coordinate authentication flow, connection setup, command execution, transfer, forwarding, and teardown
- expose reusable orchestration for copy/transfer requests that can be consumed by both `zssh copy` and future derived frontends
- supervise stream routing between protocol, renderer, and structured output sinks
- enforce backpressure policy at the orchestration level
- maintain session-oriented state needed by interactive and non-interactive commands

The session manager is the runtime authority above `libssh`. It is where product semantics live.

### 6.3 Protocol Adapter (`libssh`)

Responsibilities:

- encapsulate all direct interaction with `libssh`
- expose the primitives needed for connect, remote exec, channel I/O, copy-related operations, key handling, and proxy-related transport behavior
- translate `libssh` errors and events into internal protocol-layer results that upstream code can reason about

The protocol adapter is the only place where copy-related wire behavior is allowed to depend on `libssh`. Higher layers must consume copy behavior through internal service interfaces, not by calling protocol primitives ad hoc.

No other layer may call `libssh` directly.

### 6.4 Terminal Renderer

Responsibilities:

- render interactive terminal output
- apply the approved `Catppuccin Mocha` visual theme
- manage display state, resize behavior, and user-facing terminal presentation
- consume stream data from the session manager without taking ownership of session control

The renderer owns presentation, not transport.

### 6.5 Platform Abstraction

Responsibilities:

- wrap OS-specific eventing and terminal primitives
- provide Linux implementations over `epoll` and PTY
- provide Windows implementations over `IOCP` and `ConPTY`
- normalize filesystem, process, console, and timing differences needed by higher layers

The rest of the system must depend on platform capabilities through this abstraction rather than on raw OS APIs.

## 7. Runtime Data Flow

At a high level, runtime flow is:

1. CLI parses a command and resolves config.
2. CLI creates a normalized request and passes it to the session manager.
3. Session manager uses the protocol adapter to establish the remote session.
4. Protocol events and byte streams flow back into the session manager.
5. Session manager routes data to either the terminal renderer, the JSON emitter, or both, depending on the command mode.
6. Session manager applies backpressure policy when downstream consumers cannot keep up.
7. Session manager finalizes status and returns an exit result to the CLI.

This keeps protocol, orchestration, presentation, and platform concerns separate.

For transfer workflows, the same runtime path should also support a future `zazaki_scp` entrypoint by accepting a normalized copy request instead of assuming that `zssh` is the only caller.

## 8. Authentication Model

Authentication is a first-class design surface and must be represented consistently across CLI, YAML config, and runtime orchestration.

At the design level, the important constraints are:

- authentication configuration belongs to the user-facing YAML profile model
- authentication execution belongs to the session manager plus protocol adapter boundary
- authentication state and secret handling must not leak into renderer or platform-specific presentation code
- different commands must reuse one coherent authentication model rather than invent command-specific login behavior

Detailed per-mode mapping to `libssh` APIs is an implementation-plan concern, but the model itself is already in scope and frozen as a required subsystem.

## 9. Session Management

Session management is not a thin wrapper around a socket connection. It is the coordinating layer that gives `zssh` product behavior.

It must cover:

- connection establishment and teardown
- command execution lifecycle
- interactive session ownership
- channel and stream coordination
- copy and proxy command orchestration
- state transitions needed for deterministic human and JSON-facing output

All commands rely on this shared authority rather than each subcommand building its own connection flow from scratch.

This includes copy orchestration as a reusable service boundary, not just as logic hidden inside one CLI command handler.

## 10. Backpressure Handling

Backpressure handling is a mandatory design concern because `zssh` must bridge remote streams, local rendering, structured output, and platform event loops.

The design requirements are:

- the system must explicitly model producer and consumer boundaries between protocol I/O, session orchestration, and output sinks
- the session manager is responsible for applying bounded flow-control policy
- slow local rendering or slow JSON consumers must not silently corrupt session state
- backpressure handling must work for both interactive terminal output and machine-readable execution flows

Exact queue structures, thresholds, and wakeup strategies are implementation details to be frozen in the plan, not omitted from it.

## 11. Output Model

`zssh` has two output contracts.

### 11.1 Human-readable mode

Human-readable mode is optimized for operators using the terminal directly. It prioritizes clarity, terminal fidelity, and an integrated visual experience through the terminal renderer.

### 11.2 AI-friendly JSON mode

`--json` is a first-class output mode for automation and AI consumers.

Its design constraints are:

- machine-readable structure is stable and deliberate, not a debug dump
- command lifecycle, status, and error information must be expressible without scraping terminal text
- JSON mode must be coordinated by the same session manager used by human-readable mode
- the existence of JSON mode must influence how results, errors, and progress are modeled throughout the stack

For fully interactive terminal sessions, JSON mode reports lifecycle and result information rather than replacing terminal paint operations.

The exact schema is a plan-level artifact, but the product commitment to machine-readable output is already frozen.

Copy workflows should also participate in this output model. A future `zazaki_scp` facade should be able to reuse the same structured status and error model instead of inventing a second transfer-only output contract.

## 12. Visual Design

The approved terminal theme is `Catppuccin Mocha`.

This matters architecturally because the renderer must own theme application and keep presentation policy out of the protocol and session layers. The theme is therefore not just a cosmetic note; it is part of the renderer contract.

## 13. Cross-platform Strategy

Cross-platform support is a core requirement, not a post-porting exercise.

The design rules are:

- OS-specific eventing must be isolated behind the platform abstraction layer
- Linux behavior is built on `epoll` plus PTY primitives
- Windows behavior is built on `IOCP` plus `ConPTY`
- higher layers must depend on normalized capabilities rather than conditional logic spread throughout the codebase

This keeps the platform split localized and testable.

## 14. Build and Project Shape

The implementation phase should produce a project skeleton that reflects the approved layering.

At minimum, the skeleton must support:

- `CMake`-based builds
- dependency integration for `libssh`
- room for per-layer source directories
- room for tests and CI configuration
- Linux and Windows specific implementation units under one repository
- room for reusable runtime code that can be linked by more than one CLI frontend

The exact directory names will be finalized in the implementation plan, but the structure must follow the layer boundaries defined in this design.

## 15. Testing and Verification Intent

The design already constrains the future verification strategy.

Implementation planning must include validation for:

- command parsing and config resolution
- session lifecycle behavior
- protocol adapter correctness against `libssh`
- terminal rendering behavior where deterministic checks are possible
- backpressure behavior under slow-consumer conditions
- Linux and Windows platform abstraction behavior
- JSON output contract stability

Verification is part of the design expectation, not a final cleanup step.

## 16. Out of Scope for This Design Draft

The following items are intentionally deferred to the implementation plan rather than left ambiguous:

- exact CLI flag matrix for each command
- concrete YAML schema fields
- exact JSON schema
- internal class and namespace layout
- concrete queue types and thresholds for backpressure
- CI provider choice and workflow file details

Deferring these details is acceptable because the product shape, architecture, and subsystem boundaries are already frozen.

## 17. Review Gates

Before implementation begins:

1. This design document must be reviewed and accepted by the user.
2. The accepted design must be converted into an implementation plan.
3. Project skeleton and coding work may start only after that plan is frozen.

This preserves the agreed workflow: design first, plan second, implementation third.
