import { useEffect, useState } from "react";
import {
  forceSimulation,
  forceLink,
  forceManyBody,
  forceX,
  forceY,
  SimulationNodeDatum,
  SimulationLinkDatum,
} from "d3-force";
import {
  useReactFlow,
  ReactFlowState,
  Node,
  Edge,
  FitViewOptions,
} from "reactflow";
import { useParams } from "react-router-dom";
import { AppState } from "../../../store";
import useStore from "../../../store";
import { useShallow } from "zustand/react/shallow";
import React from "react";
import { hierarchyNode } from "../../../store_util";
import * as d3 from "d3";
import overlapRemoval from "./D3-overlapRemoval";
import { grey } from "@mui/material/colors";
import collide from "./collide";

type SimNodeType = SimulationNodeDatum & Node;

const selector = (state: AppState) => ({
  graphData: state.graphData,
  setNodes: state.setNodes,
  getGroupEdges: state.getGroupEdges,
  mergeNodes: state.mergeNodes,
});


const D3ForceLayoutHandler = (
  mainProgramNodes: Node[],
  mainProgramNodeIds: String[],
  setNodes: (graphId: number, nodes: Node[]) => void,
  mergeNodes: (graphId: number, layoutedNodes: Node[]) => Node[],
  fitView: (fitViewOptions?: FitViewOptions) => boolean,
  getIntersectingNodes: (
    node: Partial<Node> & { id: Node["id"] },
    partially?: boolean,
    nodes?: Node[]
  ) => Node[],
  setOverlapRemoval: React.Dispatch<React.SetStateAction<boolean>>,
  getGroupEdges: (graphId: number, groupNodes : Node[], edges: Edge[]) => Edge[],
  edges: Edge[]
) => {
  const graphId = Number(window.location.pathname.replace("/graphs/", ""));
  if (!mainProgramNodes.length) {
    return;
  }

  let run : Boolean = true;
  let finalStop : Boolean = false;

  setTimeout(function() {
    finalStop = true;
    setOverlapRemoval(false)
  }, 5000)

  function overlapRemoval(){

    console.log("called overlap removal")

    // 1. proof for intersections
    const intersections = mainProgramNodes.map((node) => {
      return getIntersectingNodes(node).filter((nd) =>
        mainProgramNodeIds.includes(nd.id)
      );
    });
    const intersectionCount = intersections.reduce((total, elem) => {
      return total + elem.length;
    }, 0);

    if(finalStop) {
      return () => simulation.stop;
    }

    if(!run && intersectionCount == 0) { 
      setOverlapRemoval(false);
      return () => {
        simulation.stop();
      };
    }
    else if(!run && intersectionCount > 0) {
      run = true;
    }

    // 2. if there are no intersections stop simulation (?) und return
    if (intersectionCount == 0) {
      run = false;
      setTimeout(function() {
        overlapRemoval(); 
      }, 200);
    }

    // 3. if there are intersections berechne Simulationsknoten, führe Simulation durch und rufe Layouthandler erneut auf
    let intersectingNodes: Node[] = [];
    for (let i = 0; i < intersections.length; i++) {
      intersectingNodes = intersectingNodes.concat(intersections[i]);
    }

    const simulationNodes: SimNodeType[] = intersectingNodes.map((node) => {

      let w = node.width ?? 0;
      let h = node.height ?? 0;
      let x = node.position.x ?? 0;
      let y = node.position.y ?? 0;

      return {
        ...node,
        x: x + w/2, // place midpoint in the center of the node
        y: y + h/2,
        //x: node.position.x, // place midpoint in the center of the node
        //y: node.position.y
        
      };
    });
    
    const groupEdges = getGroupEdges(graphId, mainProgramNodes, edges);
      const simulationLinks : SimulationLinkDatum<SimNodeType>[] = groupEdges.map((edge) => {
        return {...edge}
      });

    console.log(simulationNodes)

    const simulation = forceSimulation()
    .nodes(simulationNodes)
    .force(
      "collide", collide())
    .on("tick", () => {
      let layoutedNodes = simulationNodes.map((node, i) => {
        const { x, y } = simulationNodes[i];
        return { ...node, position: { x: x - node.width/2 ?? 0, y: y-node.height/2 ?? 0 } };
      });
      setNodes(graphId, mergeNodes(graphId, layoutedNodes));
    });

    simulation.tick()

    window.requestAnimationFrame(() => {
      //fitView();
      overlapRemoval();
    });
  };

  overlapRemoval();   // first call to function
};

export default function OverlapRemoval() {
  const { graphId } = useParams();
  const { fitView, getIntersectingNodes } = useReactFlow();
  const { graphData, setNodes, mergeNodes, getGroupEdges } = useStore(useShallow(selector));

  let graph = graphData[Number(graphId)];
  const [overlapRemoval, setOverlapRemoval] = useState(false);
  const handleChange = () => {

    if (!overlapRemoval) {
      setOverlapRemoval(!overlapRemoval);

      const mainProgramNodes: Node[] = graph.groups[
        graph.groups.length - 1
      ].map((node) => {
        return graph.nodes[node.index];
      });
      const mainProgramNodeIds = mainProgramNodes.map((node) => node.id);
      
      D3ForceLayoutHandler(
        mainProgramNodes,
        mainProgramNodeIds,
        setNodes,
        mergeNodes,
        fitView,
        getIntersectingNodes, 
        setOverlapRemoval,
        getGroupEdges,
        graph.edges
      );
    }
    else {
      setOverlapRemoval(!overlapRemoval);
    }
  };

  return (
    
    overlapRemoval? 
    <button onClick={handleChange} style={{background: "#e7e7e7", marginTop: 42 + 'pt', width: 300 + "px"}} >
    Kollisionskräfte
    </button>
    :
    <button style={{marginTop: 42 + 'pt', width: 300 + "px"}} onClick={handleChange}  >
    Kollisionskräfte
    </button>
)
}
