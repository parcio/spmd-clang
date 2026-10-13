import Dagre from '@dagrejs/dagre';
import useStore from '../../store';
import { getNodeDimension } from './layout_util';

import { useShallow } from 'zustand/react/shallow';
import { useReactFlow } from 'reactflow';
import { useParams } from 'react-router-dom';
import { useEffect, useRef } from 'react';

const selector = (state) => ({
    graphData: state.graphData,
    setNodes: state.setNodes,
    getBoundingBoxAndAdjustedChildren : state.getBoundingBoxAndAdjustedChildren,
    mergeNodes: state.mergeNodes,
    getGroupEdges: state.getGroupEdges, 
    selectedMode: state.selectedMode,
    selectedLayout: state.selectedLayout,
    autoLayout: state.autoLayout,
    setSelectedLayout: state.setSelectedLayout
  })

function calculateDagreLayout(groupNodes, groupEdges, direction) {
    const g = new Dagre.graphlib.Graph({compound:true}).setDefaultEdgeLabel(() => ({}));

    g.setGraph(
        { 
            rankdir: direction, 
            nodesep: 70, 
            edgesep: 50, 
            ranksep: 70 
        }
    );

    groupNodes.forEach((node) => {
        const { width, height } = getNodeDimension(node);
        g.setNode(node.id, { width, height })
    });

    groupEdges.forEach((edge) => {
        g.setEdge(edge.source, edge.target);
    });

    Dagre.layout(g);

    const layoutedNodes = groupNodes.map((node) => {
        return ({
                ...node,  
                position: {
                    x: g.node(node.id).x - g.node(node.id).width/2,
                    y: g.node(node.id).y  - g.node(node.id).height/2
                }})
    });

    return layoutedNodes;
}

export const dagreLayoutHandler = (groups, edges, setNodes, getBoundingBoxAndAdjustedChildren, mergeNodes, getGroupEdges, direction) => {

    const graphId = Number(window.location.pathname.replace("/graphs/",""));
    let groupNumber = 0; 
    let mainProgrammIndex = groups.length-1;

    groups.forEach((group) => {
        if(!group.active && group.id !== "main") {
            groupNumber++;
            return;
        }
        const data = useStore.getState().graphData[graphId].nodes

        const curGroup = group.children
            .map((node) => data[node.index])
            .filter((node) => !node.hidden);

        const groupEdges = getGroupEdges(graphId, curGroup, edges);
        const layoutedNodes = calculateDagreLayout(curGroup, groupEdges, direction);

        if(groupNumber < mainProgrammIndex) {
            setNodes(graphId, mergeNodes(graphId, getBoundingBoxAndAdjustedChildren(graphId, layoutedNodes)));
        }
        else {
            setNodes(graphId, mergeNodes(graphId, layoutedNodes));
        }
        groupNumber++;
    })
}

export default function Dagre_Layout() {

    const { graphData, setNodes, getBoundingBoxAndAdjustedChildren, mergeNodes, getGroupEdges, selectedMode, selectedLayout, setSelectedLayout, autoLayout } = useStore(useShallow(selector));
    const { graphId } = useParams();
    const { fitView } = useReactFlow();

    useEffect(() => {
        const selected = selectedLayout === 'dagre' || !selectedLayout;
        
        // Nur ausführen wenn autoLayout aktiv ist, das richtige Layout ausgewählt ist und noch nicht ausgeführt wurde
        if(autoLayout && selected) {
            dagreLayoutHandler(graphData[graphId].groups, graphData[graphId].edges, setNodes, getBoundingBoxAndAdjustedChildren, mergeNodes, getGroupEdges, 'TB');
            window.requestAnimationFrame(() => fitView());
        }
    
    }, [autoLayout, selectedLayout, selectedMode, graphId])

    return (
        <button 
          style={{
            marginTop: '5pt', 
            minWidth: '140px', 
            width: '100%', 
            maxWidth: '140px',
            fontWeight: selectedLayout === "dagre" ? "bold" : undefined,
            border: selectedLayout === "dagre" ? "2px solid #1e40af" : undefined,
            cursor: "pointer",
            padding: "0.5rem",
            borderRadius: "4px",
            transition: "all 0.2s ease"
          }} 
          onClick={() => {
            setSelectedLayout("dagre");
            dagreLayoutHandler(graphData[graphId].groups, graphData[graphId].edges, setNodes, getBoundingBoxAndAdjustedChildren, mergeNodes, getGroupEdges, 'TB');
            window.requestAnimationFrame(() => fitView());
          }}
        >
          Dagre
        </button>
    )
}