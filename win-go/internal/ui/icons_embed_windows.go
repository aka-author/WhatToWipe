//go:build windows

package ui

import _ "embed"

// Same .ico as PE / Explorer (icons/app.ico); goversioninfo uses the root copy.
//
//go:embed icons/app.ico
var appICOEmbed []byte
