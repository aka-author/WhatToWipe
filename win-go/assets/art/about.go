// Package art holds About-dialog artwork embedded in the executable.
//
// Source of truth on disk: codebase/assets/art/about-bunny.png
// (build.ps1 copies it here before go build.)
package art

import _ "embed"

//go:embed about-bunny.png
var AboutBunnyPNG []byte
