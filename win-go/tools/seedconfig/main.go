// seedconfig writes WhatToWipe.config.txt with defaults into a directory if the file is absent.
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
	if _, err := os.Stat(path); err == nil {
		return
	} else if !os.IsNotExist(err) {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	if err := config.SaveTreemap(path, config.DefaultTreemap()); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
