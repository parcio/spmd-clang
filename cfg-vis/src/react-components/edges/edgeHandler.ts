import { Node, Edge, getConnectedEdges, MarkerType } from 'reactflow';
import { hierarchyNode, nestedNode } from '../../store_util';

export function findGroupNodesRec(groups: hierarchyNode[], group : hierarchyNode[], groupNodes : hierarchyNode[]) {

  group.forEach((node) => {
    groupNodes.push(node);
    if(node.children?.length > 0) {
      findGroupNodesRec(groups, node.children, groupNodes);
    }
  })
}


// determine which Edges are going into/coming out a function group
function findGroupEdges(groupNodes : hierarchyNode[], edges: Edge[]) {
    
    let incomingEdges : Edge[] = [];
    let outgoingEdges : Edge[] = [];

    const modGroupNodes = groupNodes.map((node) => { return {...node, data: [], position: {x: 0, y: 0}} });
    const groupEdges = getConnectedEdges(modGroupNodes, edges);
  
    // fuer jede Kante prüfen, ob source- oder target-Knoten außerhalb der Gruppierung liegt
    groupEdges.forEach(edge => {
      if(groupNodes.find(node => node.id === edge.source) === undefined) {
          incomingEdges.push(edge);
      }
      if(groupNodes.find(node => node.id === edge.target) === undefined) {
          outgoingEdges.push(edge);
      }
    });
    return { incomingEdges, outgoingEdges };
  }
  
  
  export function findFunctionData(functionNodeId : string, data: Node[], groups: hierarchyNode[], edges: Edge[]) {
      
    // find group
    let group = groups.find((groupNode) => {
      if(groupNode.id === functionNodeId) {       
        return true;
    }});
    
    if(group === undefined) {
      console.error("Es konnte keine Gruppierung für den Funktionsknoten gefunden werden.");
      return;
    }

      // find all Nodes inside function
    
      let groupNodes : hierarchyNode[] = []
      findGroupNodesRec(groups, group.children, groupNodes);
      let { incomingEdges, outgoingEdges } = findGroupEdges(groupNodes, edges);
  
          // get source & target nodes which need to be reconnected after hiding group
          let sourcesIncomingEdges : string[] = incomingEdges.map((x) => x.source);
          let targetsOutgoingEdges : string[] = outgoingEdges.map((x) => x.target);

          let bridgeEdgesGroup : Edge[] = [];
        
          sourcesIncomingEdges.forEach(src => {
            targetsOutgoingEdges.forEach(tar => {   // ggf. auch anpassen (s.o.)
            
              let edg : Edge = {
                id: 'bridge-' + src + '-' + tar,
                type: 'bridge-edge',
                source: src,
                target: tar,
                markerEnd: {
                  type: MarkerType.ArrowClosed
                },
                label: functionNodeId
              };
            
              bridgeEdgesGroup.push(edg);
            });
          });
        
        return { bridgeEdgesGroup, groupNodes };
    }
 