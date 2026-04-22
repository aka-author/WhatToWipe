package config

import "time"

// ScanPathStatusUpdate is funcspec § Scanning Configuration Parameters — scanning.updateInterval (0.5 sec).
// User column "-" in the table: the program does not read this value from the file for behavior; throttle is fixed here.
const ScanPathStatusUpdate = 500 * time.Millisecond

// ScanPathUpdateIntervalFileValue is written beside treemap keys so the settings file lists the parameter (funcspec name).
// Editing this line does not change the program; User "-".
const ScanPathUpdateIntervalFileValue = "0.5 sec"
