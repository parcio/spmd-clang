import useStore from "../../store";

// Laden eines einzelnen CFGs 

export function loadGraph(graphId) {

    // Zugriff non reactive auf Store?
    const graphData = useStore.getState().graphData[graphId];
    const nodes = graphData.nodes;
    const edges = graphData.edges;

    return {nodes, edges};
}



