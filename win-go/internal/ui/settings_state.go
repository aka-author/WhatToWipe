//go:build windows

package ui

type SettingsState struct {
	states     []RowState
	committed  []string
	validating bool
	saving     bool
}

func newSettingsState(states []RowState) *SettingsState {
	s := &SettingsState{
		states:    states,
		committed: make([]string, len(states)),
	}
	s.commit()
	return s
}

func (s *SettingsState) isDirty() bool {
	for i, state := range s.states {
		if state.LastGood != s.committed[i] {
			return true
		}
	}
	return false
}

func (s *SettingsState) commit() {
	for i, state := range s.states {
		s.committed[i] = state.LastGood
	}
}

func (s *SettingsState) revert() {
	for i := range s.states {
		s.states[i].LastGood = s.committed[i]
		s.states[i].PendingValue = s.committed[i]
	}
}
