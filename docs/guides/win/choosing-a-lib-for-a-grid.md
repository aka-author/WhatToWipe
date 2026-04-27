# Choosing a Lib for a Grid

## Goal

Choose a library that can implement a true editable grid for the Settings form with strict requirements:

- Real table/grid behavior, not "grid-like" imitation.
- Direct in-cell editing.
- No detached editor area (bottom/side/floating editor).
- Strong validation and predictable keyboard editing flow.
- Practical integration into a Go desktop application.

---

## Scope Clarification

This document applies to the case where a **true grid is mandatory**.

If a true grid is required by policy or acceptance criteria, do not implement a property-sheet substitute and do not build a custom fake grid in `walk`.

---

## Evaluation Criteria

The options were evaluated against:

- True grid semantics (rows/columns, in-cell editors, stable editor lifecycle).
- Editor feature coverage (text, numeric, dropdown, color/custom).
- Validation model (blocking/revert, custom rules).
- Maintenance and ecosystem quality.
- Licensing risk for production.
- Integration cost for "settings-only replacement, keep rest of app intact".

---

## Candidates

## 1) Wails + AG Grid (Community)

Summary: Best fit when requirements are truly grid-shaped.

Pros:

- True data-grid architecture with mature in-cell editing.
- Built-in editors (text, number, select/date/etc.) and custom editor support.
- Validation supports strict blocking modes.
- Strong keyboard navigation and edit lifecycle.
- Large ecosystem, high maintenance activity, strong documentation.
- Community edition is production-usable (MIT) for core features.
- Fits "replace settings UI only" while keeping backend logic in Go.

Cons:

- Requires WebView2 runtime handling on Windows (normal for Wails apps).
- Adds frontend stack (Node tooling, JS/TS build pipeline).
- Adds substantial deployment and ops considerations in some environments (runtime install policy, enterprise lockdown scenarios).

Verdict:

- Strong escalation option for true data-grid requirements.

---

## 2) Wails + Tabulator

Summary: Good fallback.

Pros:

- True editable grid model.
- MIT license (commercial use allowed).
- Strong validation, including blocking edit flow.
- Good custom editor API.

Cons:

- Weaker ecosystem and less depth than AG Grid.
- Color editing usually requires custom editor wiring.

Verdict:

- Good option if AG Grid is not desired.

---

## 3) Handsontable

Summary: Technically capable, licensing-heavy.

Pros:

- Rich spreadsheet-like editing.
- Strong validation/edit UX.

Cons:

- Production use requires commercial/proprietary licensing.
- Free mode is non-commercial/evaluation only.

Verdict:

- Not preferred unless commercial licensing is explicitly accepted.

---

## 4) Fyne Table

Summary: Not preferred for this Windows-native app.

Pros:

- Pure Go stack.
- Cross-platform.

Cons:

- Not native Win32 look-and-feel, which is a mismatch for a Windows-native UI expectation.
- Requires extra work to emulate rich property-sheet/table editing behavior.

Verdict:

- Not preferred for this project context.

---

## 5) Qt bindings for Go (therecipe/qt and related forks)

Summary: Powerful widgets, but maintenance and integration risk.

Pros:

- Qt has real grid widgets and rich editor support.

Cons:

- Go binding ecosystem maturity/maintenance risk.
- Tooling and packaging complexity.

Verdict:

- Not recommended as first choice for this project.

---

## 6) wxWidgets Go wrappers (wxGo variants)

Summary: Native-grid capable but wrapper ecosystem risk.

Pros:

- wxGrid is a real editable grid control.

Cons:

- Wrapper maturity and ergonomics concerns.
- Higher long-term maintenance risk vs web-grid approach.

Verdict:

- Not preferred over Wails + AG Grid.

---

## Recommendation

Primary choice:

- Use Wails + AG Grid Community.

Fallback:

- Use Wails + Tabulator.

Do not use:

- Custom fake-grid implementations in `walk` (including property-sheet substitutes when true grid is mandatory).

Rationale:

- AG Grid provides real in-cell editing semantics, strong keyboard behavior, validation hooks, and mature custom editor support.
- AG Grid Community licensing is suitable for core production grid use.
- Under mandatory-grid requirements, reliability and correctness of a true grid takes precedence over minimizing stack complexity.

---

## Decision Rules

If true grid is mandatory:

1) Primary path:

- Implement settings UI in Wails + AG Grid Community.

2) Fallback path:

- If AG Grid cannot be used for non-functional reasons, use Wails + Tabulator.

3) Rejected path:

- Do not implement `walk` property-sheet or any custom fake-grid substitute.

---

## Integration Risk Notes

For a `walk` app, introducing Wails for one dialog carries substantial cost:

- New UI runtime and build toolchain are added.
- Additional packaging/deployment concerns (WebView2 strategies).
- Separate window lifecycle and frontend/backend bridge concerns.
- UX consistency risk: a web-rendered settings surface inside an otherwise native app.

Under mandatory-grid requirements, these costs are accepted trade-offs rather than blockers.

---

## Implementation Scope (Recommended)

- Replace only the Settings form UI with Wails + AG Grid Community.
- Keep the rest of the app intact.
- Keep existing Go config model, validation rules, and persistence logic.
- Implement true grid columns and in-cell editors.
- For color fields, use a custom in-cell editor that combines value input + picker behavior without detached editor areas.
- Keep actions:
  - Apply
  - OK
  - Cancel
  - Reset Treemap Defaults

---

## Why This Revision Exists

This revision reflects a strict policy change:

- True grid is mandatory.
- Property-sheet substitutions are no longer acceptable.

---

## Acceptance Checklist

- Uses a true grid control (`AG Grid`) as the editable surface.
- Every edit happens in-cell; no detached editor panel exists.
- Invalid values are blocked or reverted according to explicit rule.
- Color rows support both typed hex and picker-based update.
- Full save remains atomic: all valid or none applied.
- Settings-only replacement; other app functionality remains unchanged.
