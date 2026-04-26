// seedconfig writes canonical Trash Advisor.config.txt into a target directory from current spec defaults.
// This tool is on-demand; invoke it explicitly when you need to refresh a config file.
package main

import (
	"fmt"
	"os"
	"path/filepath"

	"trashadvisor/win-go/internal/config"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: seedconfig <directory>")
		os.Exit(2)
	}
	dir := os.Args[1]
	path := filepath.Join(dir, config.ConfigFileName)
	if _, err := os.Stat(path); err != nil && !os.IsNotExist(err) {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if err := config.SaveTreemap(path, config.DefaultTreemap()); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
