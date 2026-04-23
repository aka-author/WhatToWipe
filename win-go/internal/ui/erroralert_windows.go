//go:build windows

package ui

import (
	"fmt"
	"strings"

	"github.com/lxn/walk"
)

// Verbatim English messages from docs/specs/funcspec.md § Errors (user-facing).
const (
	specErr001 = "The folder could not be opened for scanning. Check that it still exists and that you have permission to read it."
	specErr003 = "The folder could not be opened in File Explorer."
)

// SpecErr004Message substitutes the path into the #004 message template (funcspec error table).
func SpecErr004Message(absPath string) string {
	const template = "The folder %folder path% is not found. Might you have deleted it in the meantime?"
	return strings.ReplaceAll(template, "%folder path%", absPath)
}

const specInterruptionScanningInterrupted = "Scanning was interrupted."

func (a *app) showErrorAlert(code, message string) {
	if a == nil || a.mw == nil {
		return
	}
	text := fmt.Sprintf("Error code: %s\n\n%s", code, message)
	walk.MsgBox(a.mw, "WhatToWipe", text, walk.MsgBoxOK|walk.MsgBoxApplModal|walk.MsgBoxIconError)
}

func (a *app) showInterruptionAlert(message string) {
	if a == nil || a.mw == nil {
		return
	}
	walk.MsgBox(a.mw, "WhatToWipe", message, walk.MsgBoxOK|walk.MsgBoxApplModal|walk.MsgBoxIconInformation)
}
