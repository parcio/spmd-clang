import useStore from "../store";
import {Node, Edge, useStore as useReactFlowStore} from 'reactflow';
import { EdgeLabelRenderer, useInternalNode } from '@xyflow/react';
import { useShallow } from "zustand/react/shallow";
import { useParams } from "react-router-dom";

import { dagreLayoutHandler } from "./layout-algorithms/Dagre";
import { getLayoutedElementsFromElk } from "./layout-algorithms/ELK";
import React, { useCallback } from "react";
import { keyframes } from "@emotion/react";


const selector = (state) => ({
  graphData: state.graphData,
  setNodes: state.setNodes,
  getBoundingBoxAndAdjustedChildren: state.getBoundingBoxAndAdjustedChildren,
  mergeNodes: state.mergeNodes,
  getGroupEdges: state.getGroupEdges,
});

type edgeEndPoints = {
  edgeId : string,
  startNodeId : string,
  endNodeId : string,
  x0: number,   // Startpunkt
  y0: number,
  x1: number,   // Endpunkt
  y1: number
}

function determineIntersection(edg1 : edgeEndPoints, edg2 : edgeEndPoints) {

  if(edg1.x0 == edg1.x1 && edg2.x0 == edg2.x1) {    // 2 horizontale Strecken
    return false; 
  }

  let x = 0; 
  let y = 0; 

  if(edg1.x0 == edg1.x1) {                          // edg1 horizontal
    let m2 = (edg2.y1-edg2.y0) / (edg2.x1 - edg2.x0);
    let b2 =  edg2.y0 - m2 * edg2.x0;
    x = edg1.x0;
    y = m2 * x + b2;
  }
  else if(edg2.x0 == edg2.x1) {                     // edg2 horizontal
    let m1 = (edg1.y1-edg1.y0) / (edg1.x1 - edg1.x0);
    let b1 = edg1.y0 - m1 * edg1.x0;
    x = edg2.x0;
    y = m1 * x + b1;
  }
  else {                                            // keine Strecke horizontal
    let m1 = (edg1.y1-edg1.y0) / (edg1.x1 - edg1.x0);
    let m2 = (edg2.y1-edg2.y0) / (edg2.x1 - edg2.x0);
    let b1 = edg1.y0 - m1 * edg1.x0;
    let b2 =  edg2.y0 - m2 * edg2.x0;
    x = (b1-b2) / (m2-m1);
    y = m1 * x + b1;
  }

  if(Number.isFinite(x) && Number.isFinite(y)){

    if( 
      ((edg1.x1 > x && edg1.x0 < x) || (edg1.x0 > x && edg1.x1 < x)) && 
      ((edg1.y1 > y && edg1.y0 < y) || (edg1.y0 > y && edg1.y1 < y)) && 
      ((edg2.x1 > x && edg2.x0 < x) || (edg2.x0 > x && edg2.x1 < x)) &&
      ((edg2.y1 > y && edg2.y0 < y) || (edg2.y0 > y && edg2.y1 < y)) &&
      !(edg1.startNodeId == edg2.endNodeId || edg1.endNodeId == edg2.startNodeId ||
        edg1.startNodeId == edg2.startNodeId || edg1.endNodeId == edg2.endNodeId
      )
      ) 
    {
      console.log("Exe - " + edg1.edgeId + " is intersecting " + edg2.edgeId);
      return true;
    }
    else { return false; }
  }
  else {
    console.error("Ungültiger Schnittpunkt.")
    return false;
  }
}

function calculateEdgeLength(x0: number, y0:number, x1: number, y1: number) {
  return Math.sqrt(Math.pow(x0-x1, 2) + Math.pow(y0-y1, 2));
}

let dagre_execution_times : number[] = [];


export default function TestDagreAndEdges({ ITERATIONS }) {
  const {
    graphData,
    setNodes,
    getBoundingBoxAndAdjustedChildren,
    mergeNodes,
    getGroupEdges,
  } = useStore(useShallow(selector));
  const { graphId } = useParams();
  const { elkLayoutHandler } = getLayoutedElementsFromElk(
    getGroupEdges,
    setNodes,
    mergeNodes,
    getBoundingBoxAndAdjustedChildren
  );

  const testDagre = () => {
    const start = performance.now();
    //console.profile();
    dagreLayoutHandler(
      graphData[Number(graphId)].groups,
      graphData[Number(graphId)].edges,
      setNodes,
      getBoundingBoxAndAdjustedChildren,
      mergeNodes,
      getGroupEdges,
      "TB"
    );
    //console.profileEnd();
    const end = performance.now();
    //console.log(`Exe Dagre - ${end - start} ms `);
    return end - start;
  };

  const resetCoordinates = () => {
    let _nodes = graphData[Number(graphId)].nodes.map((nd) => {
      return { ...nd, position: { x: 0, y: 0 } };
    });
    setNodes(graphId, _nodes);
  };

  const executeDagreTests = () => {
    for (let i = 0; i < 1000; i++) {
      dagre_execution_times.push(testDagre());
      resetCoordinates;
    }

    let avg =
      dagre_execution_times.reduce((acc, t) => acc + t, 0) /
      dagre_execution_times.length;
    console.log(graphId + ": Average Execution Time for Dagre is " + avg + " ms.");
  };


  const getAbsoluteCoordinates = (node: Node, nodes: Node[]) => {

    if(node.positionAbsolute != undefined) {
      return node.positionAbsolute;
    }
    else {
      let x = Number(node.position.x);
      let y = Number(node.position.y);

      while(node.parentId != undefined) {
        let parentNode = nodes.find((nd) => nd.id == node.parentId) ?? node;
        x += Number(parentNode.position.x);
        y += Number(parentNode.position.y);
        node = parentNode;
      }
      return {x, y}
    }
  }


  const executeEdgeTests = (nodes : Node[], edges: Edge[]) => {

    /*
    1. get all Endpoints of Edges (approximated at Top/Bottom-Middle of CFG-Nodes) 
    2. Y-Datastructure -> sort edgeEndpoints after y-coordinates
    2. :
        - calculate Edgelength and add to edgeLengths[]
        - test intersections by comparing endpoints
        - add Edge to insertedEdges[]
    */

    let edgeLengths : number[] = [];
    let insertedEdges : edgeEndPoints [] = [];
    let intersections = 0;
  
    edges.forEach((edg) => {
      let startNode = nodes.find((nd) => nd.id == edg.source);
      let endNode = nodes.find((nd) => nd.id == edg.target);

      if(startNode != undefined && endNode != undefined && startNode.parentId == endNode.parentId) {

        let cur : edgeEndPoints;

        if(startNode.parentId == undefined || endNode.parentId == undefined) {

          cur = {
            edgeId: edg.id,
            startNodeId: startNode.id,
            endNodeId: endNode.id,
            x0: startNode.position.x + startNode.width/2,
            y0: startNode.position.y + startNode.height,
            x1: endNode.position.x + endNode.width/2,
            y1: endNode.position.y
          }
        }
        else {
          let absPosStart = getAbsoluteCoordinates(startNode, nodes);
          let absPosEnd = getAbsoluteCoordinates(endNode, nodes);

          cur = {
            edgeId: edg.id,
            startNodeId: startNode.id,
            endNodeId: endNode.id,
            x0: absPosStart.x + startNode.width/2,
            y0: absPosStart.y + startNode.height,
            x1: absPosEnd.x + endNode.width/2,
            y1: absPosEnd.y
          }

        }



        

        edgeLengths.push(calculateEdgeLength(cur.x0, cur.y0, cur.x1, cur.y1));

        //insertedEdges.forEach((pre) => {      // alle bereits betrachteten Kanten auf Schnitt mit cur testen (Alternative: Plane Sweep)
        //  let preParentId = nodes.find((nd) => nd.id == pre.startNodeId)?.parentId;
        //  if(startNode.parentId == preParentId) {
        //    if(determineIntersection(cur, pre)) {
        //      intersections++;
        //    }
        //  }
        //})
        insertedEdges.forEach((pre) => {
          if(determineIntersection(cur, pre)) {
            intersections++;
          }
        })

        insertedEdges.push(cur);
      }
      else {
        console.error("Knotenendpunkt " + edg.source + " oder "  + edg.target + " nicht gefunden oder gruppierungsübergreifende Kante.")
      }
    })

    let lengthPerEdge = edgeLengths.reduce((acc, l) => acc + l, 0) / edgeLengths.length;

    console.log("Exe - Durchschnittliche Kantenlänge: " + lengthPerEdge);
    console.log("Exe - Anzahl der Kantenschnittpunkte: " + intersections)
  }




  return (
    <div>
      <div>
        <button
          style={{ marginTop: 10 + "px", marginLeft: 25 + "px" }}
          onClick={() => executeDagreTests()}
        >
          Start measuring Dagre execution time
        </button>
      </div>
      <button
          style={{ marginTop: 10 + "px", marginLeft: 35 + "px" }}
          onClick={() => executeEdgeTests(graphData[Number(graphId)].nodes, graphData[Number(graphId)].edges)}
      >Test edgelength and interesections</button>  
    </div>
  );
}
