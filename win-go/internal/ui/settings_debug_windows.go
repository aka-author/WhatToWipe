//go:build windows

package ui

import (
	"log"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

func showMinimalTestDialog(owner walk.Form) {
	log.Printf("DEBUG: showMinimalTestDialog called, owner nil? %v", owner == nil)

	var dlg *walk.Dialog
	err := Dialog{
		AssignTo: &dlg,
		Title:    "Minimal Test",
		MinSize:  Size{Width: 500, Height: 400},
		Layout:   VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			Composite{
				Layout: Grid{Columns: 2, Spacing: 8},
				Children: []Widget{
					Label{Text: "Row one label"},
					LineEdit{Text: "row one value"},
					Label{Text: "Row two label"},
					LineEdit{Text: "row two value"},
					Label{Text: "Row three label"},
					LineEdit{Text: "row three value"},
				},
			},
			PushButton{
				Text:      "Close",
				OnClicked: func() { dlg.Accept() },
			},
		},
	}.Create(owner)
	if err != nil {
		walk.MsgBox(owner, "Error", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}

	result := dlg.Run()
	log.Printf("DEBUG: dialog closed with result %d", result)
}

func showSchemaTestDialog(owner walk.Form) {
	var dlg *walk.Dialog
	var rows []Widget
	for i := range treemapSchemas {
		schema := &treemapSchemas[i]
		rows = append(rows, Label{Text: schema.Label})
		rows = append(rows, LineEdit{Text: schema.Key})
	}

	err := Dialog{
		AssignTo: &dlg,
		Title:    "Schema Test",
		MinSize:  Size{Width: 600, Height: 500},
		Layout:   VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			ScrollView{
				Layout: VScroll{},
				Children: []Widget{
					Composite{
						Layout:   Grid{Columns: 2, Spacing: 8},
						Children: rows,
					},
				},
			},
			PushButton{
				Text:      "Close",
				OnClicked: func() { dlg.Accept() },
			},
		},
	}.Create(owner)
	if err != nil {
		walk.MsgBox(owner, "Error", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}

	result := dlg.Run()
	log.Printf("DEBUG: schema dialog closed with result %d", result)
}
