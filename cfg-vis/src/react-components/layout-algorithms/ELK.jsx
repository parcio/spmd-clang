import ELK from "elkjs/lib/elk.bundled";
import useStore from "../../store";
import { getNodeDimension } from './layout_util';

import { useShallow } from "zustand/react/shallow";
import { useReactFlow } from "reactflow";
import { useParams } from "react-router-dom";
import { useEffect, useState } from "react";

const selector = (state) => ({
  graphData: state.graphData,
  setNodes: state.setNodes,
  getGroupEdges: state.getGroupEdges,
  mergeNodes: state.mergeNodes,
  getBoundingBoxAndAdjustedChildren: state.getBoundingBoxAndAdjustedChildren,
  autoLayout : state.autoLayout,
  selectedMode: state.selectedMode,
  selectedLayout : state.selectedLayout,
  setSelectedLayout: state.setSelectedLayout
});


const elk = new ELK();

export const getLayoutedElementsFromElk = (
  getGroupEdges,
  setNodes,
  mergeNodes,
  getBoundingBoxAndAdjustedChildren
) => {
  const defaultOptions = {
    "elk.direction": "DOWN",
    "elk.layered.spacing.nodeNodeBetweenLayers": 70,
    "elk.spacing.nodeNode": 70,
    //'elk.hierarchyHandling' : 'SEPARATE_CHILDREN'   // kann keine hierarchie-übergreifenden Kanten layouten
    "elk.hierarchyHandling": "INCLUDE_CHILDREN", // soll das eigtl lösen, aber gibt nur Layout für oberste Hierarchie zurück
  };

  const elkLayoutHandler = (
    groups,
    index,
    edges,
    options,
    variableRankSpacing,
    start
  ) => {
    const graphId = Number(window.location.pathname.replace("/graphs/", ""));

    // neue Daten aus dem Store laden
    const nodes = useStore.getState().graphData[graphId].nodes;
    let maxHeight = 0;
    let children = groups[index].children
      .map((node) => nodes[node.index])
      .filter((node) => !node.hidden)
      .map((nd) => {
        const { width, height } = getNodeDimension(nd);
        if (variableRankSpacing && height > maxHeight) maxHeight = height;
        // Stelle sicher dass die echten Dimensionen in den Knoten gesetzt sind für ELK
        return { ...nd, width, height, children: [] };     // don't let ELKjs relayout children
      });

    let groupEdges = getGroupEdges(graphId, children, edges);

    if (variableRankSpacing) {
      options = {
        ...options,
        "elk.layered.spacing.nodeNodeBetweenLayers": maxHeight, 
      };
    }
    const layoutOptions = { ...defaultOptions, ...options };

    const g = {
      id: index,
      layoutOptions: layoutOptions,
      children: children,
      edges: groupEdges,
    };

    elk.layout(g).then(({ children }) => {

      children.forEach((node) => {
        node.position = { x: node.x, y: node.y };
      });

      if (index < groups.length - 1) {
        setNodes(
          graphId,
          mergeNodes(
            graphId,
            getBoundingBoxAndAdjustedChildren(
              graphId,
              children
            )
          )
        );
        elkLayoutHandler(
          groups,
          index + 1,
          edges,
          options,
          variableRankSpacing,
          start
        );
      } else {
        setNodes(graphId, mergeNodes(graphId, children));
      }
    });
  };

  return { elkLayoutHandler };
};


export default function ELK_Layout() {
  const {
    graphData,
    getGroupEdges,
    setNodes,
    mergeNodes,
    getBoundingBoxAndAdjustedChildren,
    autoLayout,
    selectedMode,
    selectedLayout,
    setSelectedLayout
  } = useStore(useShallow(selector));
  const { elkLayoutHandler } = getLayoutedElementsFromElk(
    getGroupEdges,
    setNodes,
    mergeNodes,
    getBoundingBoxAndAdjustedChildren
  );

  const { graphId } = useParams();
  const { fitView } = useReactFlow();
  const [variableRankSpacing, setVariableRankSpacing] = useState(false);

  const handleChange = () => {
    setVariableRankSpacing(!variableRankSpacing);
  };

  useEffect(() => {
    const selected = selectedLayout === "elk-layered" || "elk-tree";
    if(autoLayout && selected) {
      const algorithm = selectedLayout === 'elk-tree' ? 'org.eclipse.elk.mrtree' : 'org.eclipse.elk.layered';
      
      elkLayoutHandler(
        graphData[graphId].groups,
        0,
        graphData[graphId].edges,
        { "elk.algorithm": algorithm, "elk.direction": "DOWN" },
        variableRankSpacing,
        performance.now()
      );
      window.requestAnimationFrame(() => fitView());
    }
  }, [autoLayout, selectedLayout, selectedMode, graphId])

  return (
    <>
    <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
      <button
        style={{ 
          marginTop: '5pt', 
          minWidth: '140px',
          width: '100%',
          maxWidth: '140px',
          fontWeight: selectedLayout === "elk-layered" ? "bold" : undefined,
          border: selectedLayout === "elk-layered" ? "2px solid #1e40af" : undefined,
          cursor: "pointer",
          padding: "0.5rem",
          borderRadius: "4px",
          transition: "all 0.2s ease"
          }}
        onClick={() => {
          elkLayoutHandler(
            graphData[graphId].groups,
            0,
            graphData[graphId].edges,
            { "elk.algorithm": "org.eclipse.elk.layered", "elk.direction": "DOWN" },
            variableRankSpacing,
            performance.now()
          );
          setSelectedLayout("elk-layered");
          window.requestAnimationFrame(() => fitView());
        }}
      >
        ELKjs - Layered
      </button>
      <button
       style={{ 
          marginTop: '5pt', 
          minWidth: '140px',
          width: '100%',
          maxWidth: '140px',
          fontWeight: selectedLayout === "elk-tree" ? "bold" : undefined,
          border: selectedLayout === "elk-tree" ? "2px solid #1e40af" : undefined,
          cursor: "pointer",
          padding: "0.5rem",
          borderRadius: "4px",
          transition: "all 0.2s ease"
          }}
        onClick={() => {
          elkLayoutHandler(
            graphData[graphId].groups,
            0,
            graphData[graphId].edges,
            { "elk.algorithm": "org.eclipse.elk.mrtree", "elk.direction": "DOWN" },
            variableRankSpacing,
            performance.now()
          );
          setSelectedLayout("elk-tree");
          window.requestAnimationFrame(() => fitView());
        }}
      >
        ELKjs - MrTree
      </button>
    </div>
  </>
  );
}
