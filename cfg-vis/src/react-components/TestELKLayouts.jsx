import ELK from "elkjs/lib/elk.bundled";
import useStore from "../store";
import { useShallow } from "zustand/react/shallow";
import { useReactFlow } from "reactflow";
import { useParams } from "react-router-dom";
import { useState } from "react";

const selector = (state) => ({
  graphData: state.graphData,
  setNodes: state.setNodes,
  getGroupEdges: state.getGroupEdges,
  mergeNodes: state.mergeNodes,
  getBoundingBoxAndAdjustedChildren: state.getBoundingBoxAndAdjustedChildren,
});

let algorithms = ["layered", "mrtree", "force", "stress"];
let executionTimes0 = [[], [], [], []];   // whole calculation (with loading times)
let executionTimes1 = [[], [], [], []];   // only algorithm execution
let executionTime = 0;

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

  const elkLayoutHandler = (groups, index, edges, options, start, it) => {

    const graphId = Number(window.location.pathname.replace("/graphs/", ""));

    // neue Daten aus dem Store laden
    const nodes = useStore.getState().graphData[graphId].nodes;
    let children = groups[index].map((node) => {
      let nd = nodes[node.index];
      return { ...nd, children: [] };     // don't let ELKjs relayout children
    });

    let groupEdges = getGroupEdges(graphId, children, edges);

    const layoutOptions = { ...defaultOptions, ...options };

    const g = {
      id: index,
      layoutOptions: layoutOptions,
      children: children,
      edges: groupEdges,
    };

    const startOnlyElk = performance.now();
    elk.layout(g).then(({ children }) => {

      executionTime += performance.now()-startOnlyElk;
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
          start,
          it
        );
      } else {
        executionTimes1[it%4].push(executionTime);
        executionTime = 0;
        setNodes(graphId, mergeNodes(graphId, children));

        let total = performance.now() - start;
        executionTimes0[it%4].push(total)

        // call next layout algorithm
        if(it < 400) {
          it++;
          executionTime = 0;
          let newOptions = {"algorithm" : algorithms[it%4]};
          elkLayoutHandler(groups, 0, edges, newOptions, performance.now(), it);
        }
        else {      // print results
          console.log("============= RESULTS FOR GRAPH " + graphId +  " ==============");
          //console.log(executionTimes0)
          //console.log(executionTimes1)
          for(let i = 0; i<executionTimes0.length; i++) {         // 0 => whole Calculation , 1 => only Layouts
            let mean0 = executionTimes0[i].reduce((acc, time) => acc + time, 0) / executionTimes0[i].length;
            let mean1 = executionTimes1[i].reduce((acc, time) => acc + time, 0) / executionTimes1[i].length;
            console.log(algorithms[i] + " took avg. " + mean0 + " ms for whole calculation and " + mean1 + " ms only for layouting.")
          }  
        }
      }
    });
  };

  return { elkLayoutHandler };
};



export default function TestELKLayouts() {
  const {
    graphData,
    getGroupEdges,
    setNodes,
    mergeNodes,
    getBoundingBoxAndAdjustedChildren,
  } = useStore(useShallow(selector));
  const { elkLayoutHandler } = getLayoutedElementsFromElk(
    getGroupEdges,
    setNodes,
    mergeNodes,
    getBoundingBoxAndAdjustedChildren
  );

  const { graphId } = useParams();

  const startTesting = () => {

    let it = 0;
    let options = {"algorithm" : algorithms[it]}
    elkLayoutHandler(graphData[graphId].groups, 0, graphData[graphId].edges, options, performance.now(), it);
  }


  return (
    
    <div>
      <button
        style={{ marginLeft: 25 +'px', marginTop: "20" + "pt" }}
        onClick={() => {
          startTesting();
        }}
      >
        Start measuring ELKjs execution times
      </button>
    </div>
  );
}
