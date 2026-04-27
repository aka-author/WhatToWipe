import React, { useEffect, useMemo, useState } from "react";
import { createRoot } from "react-dom/client";
import { AgGridReact } from "ag-grid-react";
import "ag-grid-community/styles/ag-grid.css";
import "ag-grid-community/styles/ag-theme-quartz.css";

const bindings = window?.go?.main?.SettingsApp;

function ColorEditor(props) {
  const [v, setV] = useState(props.value || "");
  const commit = (nv) => {
    setV(nv);
    props.onValueChange(nv);
  };
  return (
    <div style={{ display: "flex", gap: 6, alignItems: "center" }}>
      <input
        style={{ width: "100%" }}
        value={v}
        onChange={(e) => commit(e.target.value)}
      />
      <input
        type="color"
        value={/^#[0-9a-fA-F]{6}$/.test(v) ? v : "#000000"}
        onChange={(e) => commit(e.target.value.toUpperCase())}
      />
    </div>
  );
}

function App() {
  const [rows, setRows] = useState([]);
  const [error, setError] = useState("");

  useEffect(() => {
    if (!bindings?.LoadRows) {
      setError("Wails bindings are unavailable.");
      return;
    }
    bindings.LoadRows().then(setRows).catch((e) => setError(String(e)));
  }, []);

  const colDefs = useMemo(
    () => [
      { field: "label", headerName: "Parameter", editable: false, width: 350 },
      {
        field: "value",
        headerName: "Value",
        editable: true,
        flex: 1,
        cellEditorSelector: (p) => {
          if (p.data.type === "color") {
            return { component: ColorEditor };
          }
          return undefined;
        },
      },
    ],
    []
  );

  const onApply = async () => {
    setError("");
    try {
      await bindings.ApplyRows(rows);
    } catch (e) {
      setError(String(e));
    }
  };

  const onReset = async () => {
    setError("");
    try {
      const r = await bindings.ResetRowsToDefaults();
      setRows(r);
    } catch (e) {
      setError(String(e));
    }
  };

  return (
    <div style={{ height: "100vh", display: "flex", flexDirection: "column" }}>
      <div className="ag-theme-quartz" style={{ flex: 1 }}>
        <AgGridReact rowData={rows} columnDefs={colDefs} stopEditingWhenCellsLoseFocus />
      </div>
      <div style={{ padding: 10, display: "flex", gap: 8 }}>
        <button onClick={onReset}>Reset Treemap Defaults</button>
        <div style={{ flex: 1 }} />
        <button onClick={onApply}>Apply</button>
      </div>
      {error ? <div style={{ color: "#b00020", padding: "0 10px 10px" }}>{error}</div> : null}
    </div>
  );
}

createRoot(document.getElementById("root")).render(<App />);
