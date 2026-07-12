# Repository folder structure (rules)

This document defines how directories and specification files are organized for Erase & Rewrite. It is a house rule for contributors and tooling. It does not change product meaning; [specs/funcspec.md](./specs/funcspec.md) remains the functional source of truth.

## 1. Goals

- Keep one clear place for product-wide documentation versus target-specific technical add-ons.
- Allow additional targets (OS, GUI stack, language) without renaming existing files consumers already link to.
- Keep paths predictable so CI, reviewers, and packaging scripts can glob or hard-code a small set of locations.
- Keep source trees free of build products: executables, installers, and intermediate binaries do not live beside hand-written code.

## 2. Documentation layout (`docs/`)

### 2.1 Product-wide specifications (single tree)

All behavior and wording that apply to the product regardless of implementation live under:

`docs/specs/`

Required baseline files today:

| File | Role |
|------|------|
| `funcspec.md` | Functional specification (FS). One document for the product. Platform-neutral where possible; platform-specific UI may still be described here when it is part of the product definition. |
| `legalspec.md` | Legal and licensing specification. GPLv3 cornerstone, third-party (Qt) obligations, repository rules, installer notices. |

Nothing under `docs/specs/` should fork the functional spec per delivery target. If an implementation cannot meet FS, that is a waiver or product change in FS, not a second funcspec.

### 2.2 Target-specific technical specs (flat under `docs/specs/`)

Normative add-ons use one file per target in **`docs/specs/`**:

`docs/specs/techspec-<platform>-<stack>.md`

Informative architecture notes use the parallel name in the **same** directory:

`docs/specs/arch-<platform>-<stack>.md`

As-built implementation descriptions (when present) use:

`docs/specs/impl-<platform>-<stack>.md`

**Supported today:** Windows + Go (`techspec-win-go.md`, `arch-win-go.md`) and Windows + C++ + Qt (`techspec-win-cpp-qt.md`, `arch-win-cpp-qt.md`, `impl-win-cpp-qt.md`, `fixplan.md`). The Go module remains until the Qt target is verified against FS. Do not add spec files or repo-root implementation siblings for other OSes until those targets are actually supported.

Rules for the slug `<platform>-<stack>` in the filename:

- Use lowercase ASCII, words separated by a single hyphen.
- `<platform>` is a short OS or environment id (`win`, `darwin`, `linux`, РІР‚В¦).
- `<stack>` is the primary language or UI stack id (`go`, `swift`, РІР‚В¦).
- Do not rename an existing techspec without a repo-wide migration (grep links, CI, reviews).

If one target needs several normative add-ons (rare), use an extra hyphenated suffix (`techspec-win-go-msix.md`) or a clearly named sidecar Markdown file.

### 2.3 Implementation folder at repo root

The folder **`win-go/`** matches `techspec-win-go.md`. The Windows C++ Qt module is **`win-cpp-qt/`** (matches `techspec-win-cpp-qt.md`). When you add another supported target, add matching `techspec-*.md` / `arch-*.md` under `docs/specs/` and a same-named **`<slug>/`** folder here — do not keep empty "future" trees beforehand.

### 2.4 Verification and engineering notes

Material that exists only to satisfy verification rows (decision logs, test matrices) should live under `docs/` in a dedicated tree, for example:

`docs/verification/`

Use a name chosen by the team; the techspec may refer to it without mandating the exact path. Do not scatter verification-only Markdown in random package directories unless there is a strong reason.

## 3. Source layout (`cmd/`, `internal/`, and similar)

The active **Windows Go module** is in-repo at **`win-go/`**.

Inside this repo, first-party implementation roots are: **`win-go/`**, **`win-cpp-qt/`**, `cmd/`, `internal/`, `assets/`, `docs/verification/`, plus further `techspec-*.md` / `arch-*.md` in `docs/specs/` only when a new target is real. **Versioned icon sources** live under **`assets/icons/`**: see **`assets/icons/FS-TOOLBAR-MAP.txt`** (maps to FS toolbar + main-window icon). Default SVG set plus **`*-hc.svg`** high-contrast variants.

House rules for growth:

- Prefer `cmd/<appname>/` for shipped entry points when the tree matures.
- Prefer `internal/` for packages that must not be imported by external modules.
- Platform-specific code should be obvious: build tags (`//go:build windows`), or `internal/platform/<platform>/`, or both, as long as `go list` and reviewers can see boundaries.

Do not duplicate FS or techspec text inside source folders; link to `docs/specs/` from the root `README.md` or this document instead of scattering copies.

## 4. Build outputs and executables (not in **codebase**)

Compiled binaries must not live **inside** the `codebase/` git repository.

**Current practice:** `win-go/build.ps1` emits `Erase & Rewrite.exe` to `<ProjectRoot>/bin/win/current`, where `ProjectRoot = codebase/..`. Installer builds go to `<ProjectRoot>/delivery/win/<yyyy-MM-dd_HH-mm>`.

Rules:

- The root [`.gitignore`](../.gitignore) still ignores mistaken in-repo `bin/`, `*.exe`, and similar under **codebase**.
- Generated icons and transient `.syso` files are managed under **`win-go/`** by the current build flow.

## 5. Third-party and vendored code

Vendor or forked dependencies stay under the module that owns them, clearly separated from first-party code. Do not place product specifications inside `third_party/` trees.

## 6. Precedence reminder

For a given delivery target, interpretation order is:

1. `docs/specs/funcspec.md`
2. `docs/specs/legalspec.md` for licensing and distribution obligations
3. `docs/specs/techspec-<platform>-<stack>.md` for that target, if present
4. `docs/specs/arch-<platform>-<stack>.md` for that target, informative only
5. `docs/specs/impl-<platform>-<stack>.md` for that target, as-built informative only

`funcspec.md` is owner-edited; agents document implementation elsewhere and do not revise FS.

If a target has no techspec yet, only FS applies until one is added.

## 7. Changing these rules

Amend this file in the same pull request as any layout change that would otherwise confuse readers (moves, renames, new roots). Announce path changes in commit messages so downstream forks can follow.
