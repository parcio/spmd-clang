// eslint-disable-next-line no-unused-vars
import React, {
  useCallback,
  useEffect,
  useMemo,
  useState,
} from "react";
import ReactFlow, {
  MiniMap,
  Controls,
  Background,
  ReactFlowProvider,
} from "reactflow";
import { useShallow } from "zustand/react/shallow";

import useStore from "../store";

import CFGNode from "./nodes/CFGNode";
import FunctionNode from "./nodes/FunctionNode";
import LabledDirectedEdge from "./edges/LabledDirectedEdge";
import FloatingConnectionLine from "./edges/FloatingConnectionLine";

import GraphControls from "./controls/GraphControls.jsx";

import "reactflow/dist/style.css";
import "../css/nodes.css";
import { useParams } from "react-router-dom";
import BridgeEdge from "./edges/BridgeEdge";
import { findFunctionData } from "./edges/edgeHandler";

import hljs from 'highlight.js';
import "highlight.js/styles/github-dark.css";
import cpp from 'highlight.js/lib/languages/cpp';

//import TestELKLayouts from "./TestELKLayouts"
//import TestDagreAndEdges from "./TestDagreAndEdges";

hljs.registerLanguage('cpp', cpp);


const nodeTypes = {
  "cfg-node": CFGNode,
  "function-node": FunctionNode,
};

const edgeTypes = {
  "labled-directed-edge": LabledDirectedEdge,
  "bridge-edge": BridgeEdge,
};

const selector = (state) => ({
  graphDataPaths: state.graphDataPaths,
  graphData: state.graphData,
  loadData: state.loadData,
  setNodes: state.setNodes,
  onNodesChange: state.onNodesChange,
  updateNodeState: state.updateNodeState,
  addBridgeEdge: state.addBridgeEdge,
  deleteBridgeEdge: state.deleteBridgeEdge,
  showProcessNodes: state.showProcessNodes,
  selectedMode: state.selectedMode
});

function Flow() {
  const { graphData, onNodesChange, updateNodeState, addBridgeEdge, deleteBridgeEdge, showProcessNodes, selectedMode } = useStore(useShallow(selector));
  let { graphId } = useParams();

  const currentGraph = graphData?.[graphId];
  const processRanks = useMemo(() => {
    if (!currentGraph?.processes) {
      return [];
    }

    return currentGraph.processes.map((process) => process.mpi_rank);
  }, [currentGraph?.processes]);

  useEffect(() => {
    showProcessNodes(Number(graphId), selectedMode);
  }, [graphId, selectedMode, showProcessNodes]);


  const nodes = [...currentGraph.nodes].reverse(); // damit parentNodes childNodes nicht verdecken
  const edges = currentGraph.edges;
  const groups = currentGraph.groups;

  useEffect(() => {
    hljs.highlightAll();
  }, [currentGraph]);

  // Collapse all functions
  const handleCollapseAll = useCallback(() => {
    groups.forEach((group) => {
      if (group.id !== "main" && group.active) {
        const functionData = findFunctionData(group.id, graphData[graphId].nodes, groups, edges);
        if (functionData) {
          const { bridgeEdgesGroup, groupNodes } = functionData;
          
          // Hide the function node
          updateNodeState(graphId, group.id, true);
          
          // Hide all nodes inside the function
          groupNodes.forEach((node) => {
            updateNodeState(graphId, node.id, true);
          });
          
          // Add bridge edges
          bridgeEdgesGroup.forEach((edge) => {
            addBridgeEdge(Number(graphId), edge);
          });
        }
      }
    });
  }, [graphId, groups, edges, graphData, updateNodeState, addBridgeEdge]);

  // Expand all functions
  const handleExpandAll = useCallback(() => {
    groups.forEach((group) => {
      if (group.id !== "main" && group.active) {
        const functionData = findFunctionData(group.id, graphData[graphId].nodes, groups, edges);
        if (functionData) {
          const { bridgeEdgesGroup, groupNodes } = functionData;
          
          // Show the function node
          updateNodeState(graphId, group.id, false);
          
          // Show all nodes inside the function
          groupNodes.forEach((node) => {
            updateNodeState(graphId, node.id, false);
          });
          
          // Delete bridge edges
          bridgeEdgesGroup.forEach((edge) => {
            deleteBridgeEdge(Number(graphId), edge);
          });
        }
      }
    });
  }, [graphId, groups, edges, graphData, updateNodeState, deleteBridgeEdge]);


// , background: "#ECF2F8"
  return (
    <div style={{ width: "100vw", height: "100vh"}}>
      <ReactFlow
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        nodeTypes={nodeTypes}
        edgeTypes={edgeTypes}
        connectionLineComponent={FloatingConnectionLine}
        fitView
        defaultViewport={{
          x: typeof window !== "undefined" ? window.innerWidth / 2 : 0,
          y: typeof window !== "undefined" ? window.innerHeight / 2 : 0,
          zoom: 0
        }}
        minZoom={0.001}
      >
        <GraphControls
          processRanks={processRanks}
          handleCollapseAll={handleCollapseAll}
          handleExpandAll={handleExpandAll}
        />
        <Controls position={"bottom-left"} style={{ marginBottom: 70 + "pt", marginLeft: 180 + "pt"}}/>
        <MiniMap zoomable pannable position={"bottom-left"} style={{ marginBottom: 68 + "pt"}}/>
        <Background variant="" gap={20} size={1.5} />
      </ReactFlow>
    </div>
  );
}

// Provider needed to access internal state of the Flow (useReactFlow()) for Graph Layouting Function
export default function FlowProvider() {
  return (
    <ReactFlowProvider>
      <Flow />
    </ReactFlowProvider>
  );
}
