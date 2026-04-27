# Implementing a Two-Column Config Grid in Go on Windows using walk

## Overview

The `walk` library is the most practical choice for this task. It wraps Win32 controls
natively and gives you real Windows widgets — no rendering tricks, no embedded browser,
no .NET dependency. The approach is to build a scrollable two-column layout where the
left column holds static labels and the right column holds the appropriate editor control
for each config parameter.

The library is at `github.com/lxn/walk`.

> **Note on documentation.** walk's official documentation is sparse. The best reference
> is the source code itself at `github.com/lxn/walk`, together with the examples in the
> `examples/` subdirectory of that repository. Read those before you get stuck.

---

## Setting Up

Install walk and its Win32 bindings:

```
go get github.com/lxn/walk
go get github.com/lxn/win
```

walk requires a manifest file to activate Windows visual styles. Without it, controls
render in the Windows 2000 style. Create `app.manifest` in your project root:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
        type="win32"
        name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0"
        processorArchitecture="*"
        publicKeyToken="6595b64144ccf1df"
        language="*"/>
    </dependentAssembly>
  </dependency>
  <asmv3:application xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
    <asmv3:windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/PM</dpiAware>
    </asmv3:windowsSettings>
  </asmv3:application>
</assembly>
```

The `dpiAware` entry ensures correct rendering on HiDPI screens. Without it, Windows
scales your window as a bitmap and everything looks blurry on modern monitors.

Embed the manifest via a resource file. Create `main.rc`:

```rc
1 24 "app.manifest"
```

Add this to `main.go`:

```go
//go:generate rsrc -manifest app.manifest -o rsrc.syso
```

Install `rsrc` once:

```
go install github.com/akavel/rsrc@latest
```

Run `go generate` once to produce `rsrc.syso`. The Go linker picks it up automatically
on subsequent builds — you do not need to reference it explicitly.

Since walk is Windows-only, add a build tag at the top of every file that uses it:

```go
//go:build windows

package main
```

This prevents compilation failures on Linux and macOS.

---

## Overall Window and Layout Structure

walk's declarative API (`walk/declarative`) is much cleaner than the imperative one for
building static UIs like a config grid. The declarative API lets you describe the entire
UI as nested Go structs, which is readable and maintainable. Use it unless you have a
specific reason not to.

The outer container should be a `ScrollView` so the grid can grow beyond the window
height. Inside it, use a `Composite` with a `GridLayout` of two columns.

```go
package main

import (
    "github.com/lxn/walk"
    . "github.com/lxn/walk/declarative"
)

func main() {
    var mw *walk.MainWindow

    MainWindow{
        AssignTo: &mw,
        Title:    "Configuration",
        MinSize:  Size{Width: 400, Height: 300},
        Layout:   VBox{MarginsZero: true},
        Children: []Widget{
            ScrollView{
                Layout:        VScroll{},
                VerticalFixed: false,
                Children: []Widget{
                    Composite{
                        Layout: Grid{
                            Columns: 2,
                            Spacing: 8,
                            Margins: Margins{Left: 12, Top: 12, Right: 12, Bottom: 12},
                        },
                        Children: buildRows(),
                    },
                },
            },
        },
    }.Run()
}
```

`buildRows()` returns a `[]Widget` slice containing alternating label and editor widgets.
The two-column `GridLayout` arranges them left-to-right, top-to-bottom automatically —
the first widget goes to column 1 row 1, the second to column 2 row 1, the third to
column 1 row 2, and so on. You never specify row or column numbers manually.

If this config UI is invoked from another window rather than being the entire
application, use `Dialog` instead of `MainWindow` and add OK/Cancel buttons:

```go
var dlg *walk.Dialog

Dialog{
    AssignTo: &dlg,
    Title:    "Configuration",
    MinSize:  Size{Width: 450, Height: 350},
    Layout:   VBox{},
    Children: []Widget{
        ScrollView{ /* your grid inside here */ },
        Composite{
            Layout: HBox{},
            Children: []Widget{
                PushButton{
                    Text: "OK",
                    OnClicked: func() { dlg.Accept() },
                },
                PushButton{
                    Text:      "Cancel",
                    OnClicked: func() { dlg.Cancel() },
                },
            },
        },
    },
}.Run()
```

`dlg.Accept()` and `dlg.Cancel()` close the dialog and return control to the caller.
Use `MainWindow` when config is the whole app; use `Dialog` when it is invoked from one.

---

## Defining Config Parameters

Define your config params as a slice of structs so you can iterate over them and build
the UI generically:

```go
type ParamKind int

const (
    KindText ParamKind = iota
    KindNumeric
    KindDropdown
    KindColor
)

type Param struct {
    Label   string
    Kind    ParamKind
    Options []string // only used for KindDropdown
    Min     float64  // only used for KindNumeric
    Max     float64  // only used for KindNumeric
    Value   string   // current value, always stored as string
}

var params = []Param{
    {Label: "Display name",     Kind: KindText,     Value: "My App"},
    {Label: "Log level",        Kind: KindDropdown, Options: []string{"Debug", "Info", "Warning", "Error"}, Value: "Info"},
    {Label: "Max connections",  Kind: KindNumeric,  Min: 1, Max: 9999, Value: "10"},
    {Label: "Background color", Kind: KindColor,    Value: "#FFFFFF"},
    {Label: "Foreground color", Kind: KindColor,    Value: "#000000"},
}
```

Storing all values as strings is the simplest approach for a config UI. Convert to the
appropriate type when you actually use the config, not in the UI layer.

---

## Building the Rows

```go
func buildRows() []Widget {
    var widgets []Widget
    for i := range params {
        p := &params[i] // pointer — callbacks must write back to the slice, not a copy
        widgets = append(widgets, Label{
            Text:    p.Label,
            MinSize: Size{Width: 160},
        })
        widgets = append(widgets, editorFor(p))
    }
    return widgets
}

func editorFor(p *Param) Widget {
    switch p.Kind {
    case KindText:
        return textEditor(p)
    case KindNumeric:
        return numericEditor(p)
    case KindDropdown:
        return dropdownEditor(p)
    case KindColor:
        return colorEditor(p)
    default:
        return Label{Text: "unknown"}
    }
}
```

> **Critical: always use `&params[i]`, not `p := params[i]`.** If you take a value copy,
> the callbacks will write to the copy and the original slice will never be updated.
> This is a frequent source of bugs in walk code and produces no compiler warning.

---

## The Four Editor Types

### Simple Text Edit

A `LineEdit` is walk's single-line text input. It is a real Win32 `EDIT` control —
clipboard shortcuts, undo, selection, and all standard keyboard behavior work out of the
box. It is editable by default. No special setup is needed.

```go
func textEditor(p *Param) Widget {
    var le *walk.LineEdit
    return LineEdit{
        AssignTo:          &le,
        Text:              p.Value,
        OnEditingFinished: func() { p.Value = le.Text() },
    }
}
```

Use `OnEditingFinished` rather than `OnTextChanged`. The former fires when the user
leaves the field; the latter fires on every keystroke. For a config UI, per-keystroke
updates are unnecessary and make validation awkward.

For fields where format matters, validate in `OnEditingFinished` and revert on error:

```go
OnEditingFinished: func() {
    _, err := strconv.Atoi(le.Text())
    if err != nil {
        walk.MsgBox(nil, "Invalid value", "Please enter a whole number.", walk.MsgBoxIconWarning)
        le.SetText(p.Value) // revert to last accepted value
        return
    }
    p.Value = le.Text()
},
```

### Numeric Spin Control

For numeric params, a `NumberEdit` (Win32 spinner / updown control) is more appropriate
than a plain `LineEdit`. The user gets up/down buttons, automatic clamping to min/max,
and keyboard increment with arrow keys. No manual validation is needed for the numeric
constraint itself.

```go
func numericEditor(p *Param) Widget {
    var ne *walk.NumberEdit
    val, _ := strconv.ParseFloat(p.Value, 64)
    return NumberEdit{
        AssignTo: &ne,
        Value:    val,
        MinValue: p.Min,
        MaxValue: p.Max,
        Decimals: 0,
        OnValueChanged: func() {
            p.Value = strconv.Itoa(int(ne.Value()))
        },
    }
}
```

Set `Decimals: 0` for integer params. Set it to a positive number if you need decimal
input. `MinValue` and `MaxValue` come from the `Param` struct, so each numeric param can
have its own range.

### Dropdown / ComboBox

walk's `ComboBox` maps directly to the Win32 `COMBOBOX` control. When `Editable` is
false, it is a strict dropdown — the user can only pick from the list. When `Editable`
is true, the user can also type a custom value.

```go
func dropdownEditor(p *Param) Widget {
    var cb *walk.ComboBox

    currentIndex := 0
    for i, o := range p.Options {
        if o == p.Value {
            currentIndex = i
            break
        }
    }

    return ComboBox{
        AssignTo:     &cb,
        Model:        p.Options,
        CurrentIndex: currentIndex,
        Editable:     false,
        OnCurrentIndexChanged: func() {
            idx := cb.CurrentIndex()
            if idx >= 0 && idx < len(p.Options) {
                p.Value = p.Options[idx]
            }
        },
    }
}
```

If you set `Editable: true`, also handle free-text input:

```go
OnEditingFinished: func() {
    p.Value = cb.Text()
},
```

### Color Editor (Text + Button)

This is the most involved case. The correct approach is to place a `LineEdit` and a
small `PushButton` side by side inside a horizontal `Composite`. The user can either
type a hex color directly into the edit field, or click the button to open the standard
Windows color picker dialog. Both paths write to `p.Value` and stay in sync.

```go
var hexColorRE = regexp.MustCompile(`^#[0-9A-Fa-f]{6}$`)

func colorEditor(p *Param) Widget {
    var le *walk.LineEdit
    var btn *walk.PushButton

    return Composite{
        Layout: HBox{MarginsZero: true, Spacing: 4},
        Children: []Widget{
            LineEdit{
                AssignTo: &le,
                Text:     p.Value,
                OnEditingFinished: func() {
                    if hexColorRE.MatchString(le.Text()) {
                        p.Value = le.Text()
                    } else {
                        walk.MsgBox(nil, "Invalid color", "Use format #RRGGBB", walk.MsgBoxIconWarning)
                        le.SetText(p.Value) // revert
                    }
                },
            },
            PushButton{
                AssignTo: &btn,
                Text:     "...",
                MaxSize:  Size{Width: 30},
                OnClicked: func() {
                    color := showColorDialog(btn.Handle(), p.Value)
                    if color != "" {
                        p.Value = color
                        le.SetText(color)
                    }
                },
            },
        },
    }
}
```

Note that `hexColorRE` is compiled once at package level, not inside the callback.
Compiling a regex on every keystroke or focus event is wasteful.

The `MaxSize: Size{Width: 30}` keeps the button small. The `HBox` with `Spacing: 4`
puts the two controls side by side with a small gap.

> **On the correct UX pattern.** Some walk implementations open a panel at the bottom
> of the window when the user clicks a color cell. This is wrong. It is non-standard,
> it breaks the spatial relationship between the control and its value, and users do not
> expect it. The correct behavior is: the editor is always visible in the cell, the
> dialog opens over it when the user clicks the button, and the result writes back into
> the edit field. The cell is always the source of truth.

---

## The Windows Color Picker Dialog

walk does not wrap `ChooseColor` directly, so you call it through the `win` package.
`ChooseColor` opens the standard system color picker — the same dialog used by Paint,
WordPad, and most native Windows applications.

```go
func showColorDialog(hwnd win.HWND, current string) string {
    var r, g, b uint8
    fmt.Sscanf(current, "#%02x%02x%02x", &r, &g, &b)

    // Win32 COLORREF is 0x00BBGGRR — note the byte order
    initColor := win.COLORREF(uint32(r) | uint32(g)<<8 | uint32(b)<<16)

    var customColors [16]win.COLORREF

    cc := win.CHOOSECOLOR{
        LStructSize:  uint32(unsafe.Sizeof(win.CHOOSECOLOR{})),
        HwndOwner:    hwnd,
        RgbResult:    initColor,
        LpCustColors: &customColors[0],
        Flags:        win.CC_FULLOPEN | win.CC_RGBINIT,
    }

    if win.ChooseColor(&cc) {
        r := byte(cc.RgbResult & 0xFF)
        g := byte((cc.RgbResult >> 8) & 0xFF)
        b := byte((cc.RgbResult >> 16) & 0xFF)
        return fmt.Sprintf("#%02X%02X%02X", r, g, b)
    }

    return "" // user clicked Cancel
}
```

- `CC_RGBINIT` pre-selects the current color so the dialog opens showing the existing
  value rather than an arbitrary default.
- `CC_FULLOPEN` opens the dialog fully expanded with the hue/saturation/luminosity
  controls visible. Without it, the user has to click "Define Custom Colors" to reach
  the full picker.
- `HwndOwner` makes the dialog modal relative to your window. Pass the button's handle,
  not zero. If you pass zero the dialog is not modal and the user can interact with the
  main window while the picker is open, which leads to confusing state.
- Win32 `COLORREF` stores color as `0x00BBGGRR`, not `0x00RRGGBB`. The bit shifts in
  both directions above are correct and intentional.

If your version of `lxn/win` does not include `ChooseColor` and `CHOOSECOLOR`, declare
the syscall manually:

```go
var chooseColorProc = syscall.NewLazyDLL("comdlg32.dll").NewProc("ChooseColorW")

func ChooseColor(cc *CHOOSECOLOR) bool {
    ret, _, _ := chooseColorProc.Call(uintptr(unsafe.Pointer(cc)))
    return ret != 0
}
```

---

## Making Cells Editable — The Core Principle

This is the point that many walk-based implementations get wrong, so it deserves its own
section.

The editor controls — `LineEdit`, `NumberEdit`, `ComboBox`, the color composite — must
live directly inside the grid cells as permanent, always-visible widgets. They are
created once when the window is built and stay there for the lifetime of the window.
They are not created on demand when the user clicks a cell.

This is the correct mental model: **the right column is not a display column. It is a
column of live controls.** The user can interact with any of them at any time without
any extra click to activate the cell.

walk makes this natural. When you place a `LineEdit` in a grid cell via the declarative
API, it is a real Win32 `EDIT` control sitting in that cell. The user clicks it and
types. No special handling of click events is needed, because there is no activation
step.

If you find yourself writing code that listens for a click on a cell and then creates,
shows, or moves an editor in response — stop. That is the wrong approach. Build the
editors into the layout from the start and leave them there.

---

## Column Width and Stretching

walk's `GridLayout` does not support percentage-based column widths. The left column
width is determined by the `MinSize` on the labels. Set it to a value wide enough for
your longest description:

```go
Label{
    Text:    p.Label,
    MinSize: Size{Width: 160},
}
```

The right column stretches automatically to fill remaining horizontal space — walk gives
the last column whatever space is left after fixed-width columns are satisfied. You do
not need to configure this explicitly.

All labels should use the same `MinSize.Width` so the left column is consistent.

---

## Scrolling

Wrap the grid `Composite` in a `ScrollView` as shown above. Set `VerticalFixed: false`
so the inner composite is allowed to grow taller than the window. The scrollbar appears
automatically when content exceeds the visible area.

Do not set a fixed height on the inner `Composite`. Let it size itself based on its
children. A fixed height prevents the composite from growing as rows are added.

---

## Reading Values Back

Since the callbacks write directly to the `Param` structs via pointers, reading the
final config state is just iterating over the slice:

```go
func collectConfig() map[string]string {
    result := make(map[string]string)
    for _, p := range params {
        result[p.Label] = p.Value
    }
    return result
}
```

Call this when the user clicks OK or Save. No need to query the controls — the slice is
always up to date.