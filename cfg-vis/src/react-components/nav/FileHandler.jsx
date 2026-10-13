import React, { useRef } from "react";
import useStore  from "../../store";
import { useShallow } from "zustand/react/shallow";
import { useParams, useNavigate } from "react-router-dom";

const selector = (state) => ({
  loadData: state.loadData,
  addProcessInformation : state.addProcessInformation,
  cfgFiles: state.cfgFiles,
  updateData: state.updateData,
  rename: state.rename,
  removeData: state.removeData,
  cgiFiles: state.cgiFiles,
  selectedMode: state.selectedMode,
  showProcessNodes: state.showProcessNodes
});

const options = {
  types: [
    {
      description: "JSON",
      accept: {
        "application/json": [".json"]
      },
    },
  ],
  excludeAcceptAllOption: true,
  multiple: true,
};

export default function FileHandler() {
  const fileInputRef = useRef(null);
  const navigate = useNavigate();
  let { graphId } = useParams();
  const { loadData, addProcessInformation, updateData, cfgFiles, rename, removeData, cgiFiles, selectedMode, showProcessNodes } = useStore(useShallow(selector));

  const selectedGraph = graphId ? cfgFiles[graphId] : null;
  const hasSelectedGraph = Boolean(selectedGraph);

  const processFile = async (file, fileHandle) => {
    try {
        const content = await file.text();

        // check if file contains cfg or cgi information
        if (file.name.includes("_cfg.")) {
          // rückparsen in JSON
          const graph = JSON.parse(content);
          if(!graph) {
            console.error("Daten der Datei " + file.name + " konnten nicht in JSON-Format geparsed werden.");
            return;
          }

          // Multiedges behandeln und unverbundene Knoten entfernen
          let edgesSinMultiedges = [];
          graph.edges.forEach((edg) => {
            let alreadyExistingEdge = edgesSinMultiedges.find((e) => e.id === edg.id);
            if(alreadyExistingEdge != undefined) {
              // nur label hinzufügen
              alreadyExistingEdge.label += "\n" + edg.label;
            }
            else{
                edgesSinMultiedges.push(edg);
            }
        });
        // Weitergabe an den Zustandssspeicher
        loadData(file.name, graph.nodes, edgesSinMultiedges, fileHandle);
      }

      if (file.name.includes("_cgi.")) {
        const parsed = JSON.parse(content);
        const processes = Array.isArray(parsed?.processes) ? parsed.processes : [];

        if (processes.length === 0) {
          console.error("Daten der Datei " + file.name + " enthalten kein gültiges processes-Array.");
          return;
        }
        addProcessInformation(file.name, processes, fileHandle);
      }
    } catch (error) {
      console.error(`Die Datei ${file.name} konnte nicht verarbeitet werden.`, error);
    }
  };

  const handleClick = async () => {
    if (typeof window.showOpenFilePicker === "function") {
      try {
        const fileHandles = await window.showOpenFilePicker(options);
        await Promise.all(
          fileHandles.map(async (fileHandle) => {
            const file = await fileHandle.getFile();
            await processFile(file, fileHandle);
          })
        );
      } catch (error) {
        if (error.name !== "AbortError") {
          console.error("Die Dateiauswahl ist fehlgeschlagen.", error);
        }
      }
      return;
    }

    // Firefox and other browsers without the File System Access API use a standard file input.
    fileInputRef.current?.click();
  };

  const handleInputChange = async (event) => {
    const files = Array.from(event.target.files ?? []);
    // Reset so the same files can be selected again later.
    event.target.value = "";

    await Promise.all(
      files.map((file) => processFile(file, { getFile: async () => file }))
    );
  };

  const reloadFiles = async () => {

    const graphCFGFile = graphId ? cfgFiles[graphId] : null;
    const graphCGIFile = graphId ? cgiFiles[graphId] : null;

    if (!graphCFGFile?.fileHandle) {
      console.warn("Kein FileHandle für diesen Graphen gespeichert.");
      return;
    }

    const file = await graphCFGFile.fileHandle.getFile();
    const content = await file.text();
    const graph = JSON.parse(content);

    // Multiedges behandeln und unverbundene Knoten entfernen
    let edgesSinMultiedges = [];
    graph.edges.forEach((edg) => {
      let alreadyExistingEdge = edgesSinMultiedges.find((e) => e.id === edg.id);
      if(alreadyExistingEdge != undefined) {
        // nur label hinzufügen
        alreadyExistingEdge.label += "\n" + edg.label;
      }
      else{
          edgesSinMultiedges.push(edg);
      }
    });

    if (!graphCGIFile?.fileHandle) {
      console.warn("Kein FileHandle für CGI-File dieses Graphen gespeichert.");
      return;
    }

    const _cgiFile = await graphCGIFile.fileHandle.getFile();
    const _cgiContent = await _cgiFile.text();
    const parsed = JSON.parse(_cgiContent);
    const processes = Array.isArray(parsed?.processes) ? parsed.processes : [];

    // Graph ersetzen statt neuen hinzufügen
    updateData(Number(graphId), graph.nodes, edgesSinMultiedges, processes);

    // only show the nodes of the selected mode after reloading
    showProcessNodes(Number(graphId), selectedMode);
  };  

  return (
    <div>
      <div className="sidebar-toolbar">
        <input
          ref={fileInputRef}
          type="file"
          accept=".json,application/json"
          multiple
          hidden
          onChange={handleInputChange}
        />
        <button
          type="button"
          onClick={handleClick}
          className="icon-button"
          title="Add Graphdata"
          aria-label="Add Graphdata"
        >
          +
        </button>
        <button
          type="button"
          onClick={reloadFiles}
          className="icon-button"
          title="Reload Graphdata"
          aria-label="Reload Graphdata"
          disabled={!hasSelectedGraph || !selectedGraph?.fileHandle}
        >
          ⟳
        </button>
        <button
          type="button"
          className="icon-button"
          title="Rename Graphdata"
          aria-label="Rename Graphdata"
          disabled={!hasSelectedGraph}
          onClick={() => {
            if (!selectedGraph) return;
            const newName = window.prompt("Enter new name:", selectedGraph.fileName);
            if (newName && newName !== selectedGraph.fileName) {
              rename(graphId, newName);
            }
          }}
        >
          ✎
        </button>
        <button
          type="button"
          className="icon-button"
          title="Delete Graphdata"
          aria-label="Delete Graphdata"
          disabled={!hasSelectedGraph}
          onClick={() => {
            if (!selectedGraph) return;
            if (window.confirm(`Are you sure you want to delete the graph ${selectedGraph.fileName}?`)) {
              removeData(selectedGraph.graphId);
              navigate("/");
            }
          }}
        >
          🗑
        </button>
      </div>
    </div>
  );
}

