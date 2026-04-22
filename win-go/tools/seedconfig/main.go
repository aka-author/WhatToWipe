// seedconfig writes canonical WhatToWipe.config.txt into a directory (treemap from existing file or defaults, plus scanning.updateInterval).
// build.ps1 runs this after every build so dist/ and bin/win always list scanning.updateInterval.
package main

import (
	"fmt"
	"os"
	"path/filepath"

	"whatrwipe/win-go/internal/config"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: seedconfig <directory>")
		os.Exit(2)
	}
	dir := os.Args[1]
	path := filepath.Join(dir, config.ConfigFileName)

	t := config.DefaultTreemap()
	if _, err := os.Stat(path); err == nil {
		var err error
		t, err = config.LoadTreemapFromPath(path)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
	} else if !os.IsNotExist(err) {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if err := config.SaveTreemap(path, t); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
