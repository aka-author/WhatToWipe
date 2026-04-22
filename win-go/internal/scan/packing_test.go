package scan

import (
	"testing"

	"whatrwipe/win-go/internal/model"
)

func TestClassifyArchiveLayout(t *testing.T) {
	tests := []struct {
		name string
		ents []arcEntry
		want model.PackingType
	}{
		{"empty", nil, model.PackingPackedClump},
		{"empty filtered", []arcEntry{{name: ""}}, model.PackingPackedClump},
		{"single file", []arcEntry{{name: "readme.txt", isDir: false}}, model.PackingPackedFile},
		{"two root files", []arcEntry{{name: "a.txt"}, {name: "b.txt"}}, model.PackingPackedClump},
		{"folder and file under", []arcEntry{{name: "Doc/readme.txt", isDir: false}}, model.PackingPackedFolder},
		{"two top segments", []arcEntry{{name: "a/x"}, {name: "b/y"}}, model.PackingPackedClump},
		{"dir only", []arcEntry{{name: "Empty/", isDir: true}}, model.PackingPackedFolder},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := classifyArchiveLayout(tt.ents)
			if got != tt.want {
				t.Fatalf("classifyArchiveLayout() = %v, want %v", got, tt.want)
			}
		})
	}
}
