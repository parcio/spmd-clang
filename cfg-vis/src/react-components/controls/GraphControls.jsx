import React, { useEffect, useRef, useState } from "react";
import Dagre_Layout from "../layout-algorithms/Dagre";
import ELK_Layout from "../layout-algorithms/ELK";
import useStore from "../../store";
import { useShallow } from "zustand/react/shallow";

const selector = (state) => ({
  selectedLayout: state.selectedLayout,
  autoLayout: state.autoLayout,
  selectedMode: state.selectedMode,
  setSelectedLayout: state.setSelectedLayout,
  setAutoLayout: state.setAutoLayout,
  setSelectedMode: state.setSelectedMode,
});

export default function GraphControls({processRanks, handleCollapseAll, handleExpandAll}) {
  const { selectedLayout, autoLayout, selectedMode, setSelectedLayout, setAutoLayout, setSelectedMode } = useStore(useShallow(selector));
  const [menuPosition, setMenuPosition] = useState({ x: 290, y: 24 });
  const [dragging, setDragging] = useState(false);
  const [menuCollapsed, setMenuCollapsed] = useState(false);
  const dragOffset = useRef({ x: 0, y: 0 });

  const startDrag = (event) => {
    if (event.button !== 0) return;
    setDragging(true);
    dragOffset.current = {
      x: event.clientX - menuPosition.x,
      y: event.clientY - menuPosition.y,
    };
    event.preventDefault();
  };

  const onDrag = (event) => {
    if (!dragging) return;
    setMenuPosition({
      x: Math.max(12, Math.min(window.innerWidth - 320, event.clientX - dragOffset.current.x)),
      y: Math.max(12, Math.min(window.innerHeight - 260, event.clientY - dragOffset.current.y)),
    });
  };

  const endDrag = () => {
    setDragging(false);
  };

  const toggleMenuCollapsed = () => {
    setMenuCollapsed((prev) => !prev);
  };

  useEffect(() => {
    if (!dragging) {
      return undefined;
    }

    const handleMouseMove = (event) => onDrag(event);
    const handleMouseUp = () => endDrag();

    window.addEventListener("mousemove", handleMouseMove);
    window.addEventListener("mouseup", handleMouseUp);

    return () => {
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
    };
  }, [dragging]);

  return (
    <div
      className={`floating-menu${dragging ? " dragging" : ""}${menuCollapsed ? " collapsed" : ""}`}
      style={{ left: menuPosition.x, top: menuPosition.y }}
    >
      <div className={`floating-menu-header${dragging ? " dragging" : ""}`} onMouseDown={startDrag}>
        <div>
          <div className="floating-menu-title">Graph Controls</div>
        </div>
        <button
          type="button"
          className="floating-menu-toggle"
          onClick={(event) => {
            event.stopPropagation();
            toggleMenuCollapsed();
          }}
        >
          {menuCollapsed ? "▾" : "▴"}
        </button>
      </div>
      {/* Layout-Komponenten IMMER rendern, damit useEffect läuft - aber nur Buttons zeigen wenn Menü offen */}
      <div style={{ display: menuCollapsed ? "none" : "flex", gap: "0.75rem", alignItems: "center" }}>
        <Dagre_Layout/>
        <div style={{ display: "flex", alignItems: "center", gap: "0.5rem" }}>
          <input
            type="checkbox"
            id="autoLayout"
            checked={autoLayout}
            onChange={(e) => setAutoLayout(e.target.checked)}
            style={{ cursor: "pointer" }}
          />
          <label htmlFor="autoLayout" style={{ fontSize: "12pt", cursor: "pointer", whiteSpace: "nowrap" }}>
            Auto Layout
          </label>
        </div>
      </div>
      <div style={{ display: menuCollapsed ? "none" : "block" }}>
        <ELK_Layout/>
      </div>
      {!menuCollapsed && (
        <div>
          <div style={{ marginTop: "1rem" }}>
            <div style={{ fontSize: "12pt", fontWeight: 700, marginBottom: "0.75rem" }}>
              Adjustments
            </div>
            <div style={{ display: "flex", gap: "0.75rem", alignItems: "center" }}>
              <button style={{ flex: 1, padding: "0.5rem" }} onClick={handleCollapseAll}>Collapse</button>
              <button style={{ flex: 1, padding: "0.5rem" }} onClick={handleExpandAll}>Expand</button>
              <span style={{ marginLeft: "0.75rem", whiteSpace: "nowrap" }}>all functions</span>
            </div>
          </div>
          <div style={{ marginTop: "1rem" }}>
            <div style={{ fontSize: "12pt", fontWeight: 700, marginBottom: "0.75rem" }}>
              Process Information
            </div>
            <div style={{ display: "flex", gap: "0.75rem", alignItems: "center" }}>
              <label htmlFor="ranks">Select Process Rank:</label>
              <select
                name="ranks"
                id="ranks"
                value={selectedMode}
                onChange={(event) => setSelectedMode(event.target.value)}
                style={{ padding: "0.5rem", minWidth: "140px", fontSize: "1rem" }}
              >
                <option value="overview">overview</option>
                <option value="side-by-side">side-by-side</option>
                {processRanks.map((rank) => (
                  <option key={rank} value={rank}>
                    {rank}
                  </option>
                ))}
              </select>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
