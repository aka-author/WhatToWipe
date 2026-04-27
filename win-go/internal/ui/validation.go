//go:build windows

package ui

import (
	"fmt"
	"strconv"
	"strings"
)

type ValidationError struct {
	Key     string
	Label   string
	Message string
}

func validateField(state *RowState) error {
	if state == nil || state.Schema == nil {
		return fmt.Errorf("internal error")
	}
	val := strings.TrimSpace(state.PendingValue)
	switch state.Schema.Kind {
	case KindText:
		if state.Schema.Regex != nil && val != "" && !state.Schema.Regex.MatchString(val) {
			return fmt.Errorf("%s contains invalid characters", state.Schema.Label)
		}
		if (state.Schema.Key == "treemap.labelPlaceholder" || state.Schema.Key == "treemap.labelDummy") && val == "" {
			return fmt.Errorf("%s must not be empty", state.Schema.Label)
		}
		return nil
	case KindNumeric:
		if val == "" {
			return fmt.Errorf("%s must not be empty; enter a numeric value", state.Schema.Label)
		}
		f, err := strconv.ParseFloat(val, 64)
		if err != nil {
			return fmt.Errorf("%s must be a number", state.Schema.Label)
		}
		if f < state.Schema.Min || f > state.Schema.Max {
			return fmt.Errorf("%s must be between %g and %g", state.Schema.Label, state.Schema.Min, state.Schema.Max)
		}
		return nil
	case KindDropdown:
		if strings.TrimSpace(val) == "" {
			return fmt.Errorf("%s must not be empty", state.Schema.Label)
		}
		return nil
	case KindColor:
		if _, err := parseHexColor(val); err != nil {
			return fmt.Errorf("%s must be in #RRGGBB format", state.Schema.Label)
		}
		return nil
	default:
		return fmt.Errorf("unsupported field type")
	}
}

func validateAll(states []RowState) []ValidationError {
	var out []ValidationError
	for i := range states {
		if err := validateField(&states[i]); err != nil {
			out = append(out, ValidationError{
				Key:     states[i].Schema.Key,
				Label:   states[i].Schema.Label,
				Message: err.Error(),
			})
		}
	}
	return out
}

// validateObject checks cross-field constraints across the full row state.
// Currently no cross-field rules exist. Add rules here when required.
// Called after validateAll passes, before statesToTreemap.
func validateObject(states []RowState) []ValidationError {
	return nil
}
