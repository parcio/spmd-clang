import { create } from "zustand";
import {
    addEdge,
    applyNodeChanges,
  getNodesBounds,
  Node,
  Edge,
  NodeChange,
  OnNodesChange,
  } from 'reactflow';

import { cfgFile, hierarchyNode, process as process_graph_data, graphData, nestedNode, functionDimension, process } from "./store_util";
import { node } from "prop-types";

export type AppState = {
    cfgFiles: cfgFile[],
    cgiFiles : cfgFile[],
    graphData : graphData[],
    selectedLayout: string,
    autoLayout: boolean,
    selectedMode: string,
    setSelectedLayout: (layout: string) => void,
    setAutoLayout: (enabled: boolean) => void,
    setSelectedMode: (mode: string) => void,
    loadData: (fileName : string, graphNodes : [], graphEdges : [], fileHandle : FileSystemFileHandle) => void;
    addProcessInformation: (fileName : string, processes : process[], fileHandle : FileSystemFileHandle) => void;
    showProcessNodes: (graphId: number, selectedMode: string) => void;
    updateData: (graphId: number, graphNodes : [], graphEdges : [], graphProcesses : process[]) => void;
    rename: (graphId:number, fileName :string) => void;
    removeData: (graphId:number) => void;
    setNodes : (graphId: number, nodes: Node[] ) => void;
    mergeNodes: (graphId: number, layoutedNodes: Node[]) => Node[];
    onNodesChange : OnNodesChange;
    updateNodeState: (graphId : number, nodeId : string, state: boolean) => void;
    getBoundingBoxAndAdjustedChildren : (graphId : number, layoutedNodes : (Node|nestedNode)[], functionDimensions?: functionDimension[]) => Node[];
    getGroupEdges: (graphId: number, groupNodes : Node[], edges: Edge[]) => Edge[];
    addBridgeEdge: (graphId : number, edgeToAdd: Edge) => void;
    deleteBridgeEdge: (graphId : number, edgeToDelete : Edge) => void;
}

function process_graph_data(nodes : (Node | nestedNode)[]) {

  const map = new Map(nodes.map(n => [n.id, { ...n, children: [] as hierarchyNode[]}]));
  const groups: hierarchyNode[] = [];
  const data: (Node | nestedNode)[] = [];

  const main : hierarchyNode = {
    id : "main",
    index : -1,
    children : [],
    parentId : null
  }

  for (const node of map.values()) {

    node.index = data.length;
    node.active = true;     // for activating/deactivating process and non-process nodes
    data.push(node)

    // node is part of a function
    if (node.parentId) {
      map.get(node.parentId)?.children.push(node);
    } 
    // node is part of the main function or function
    else {
      main.children.push(node);
    }
    if(node.type == "function-node") {
      groups.push(node);
    }  
  }

  // ordering the groups --> every child has to be before its function parent
  let ordered = false;

  do {
    ordered = true;

    for (let i = 0; i < groups.length; i++) {
      const child = groups[i];
      const parentId = child.parentId;

      if (parentId == null) continue;

      const index_child = i;
      const index_parent = groups.findIndex(g => g.id === parentId);

      if(index_parent === -1) {
        console.error("Error while sorting groups: parent function " + parentId + " for node " + child.id + " not found in groups array.")
        break;
      }

      if (index_child < index_parent) {
        continue;
      }
      // place childnode in front of its parentNode
      else {
        const [removed] = groups.splice(index_child, 1);
        groups.splice(index_parent, 0, removed);

        ordered = false;
        break;
      }
    }
  } while(!ordered)

  groups.push(main)
  return {data, groups};
}

function calculateBoundingBoxAndAdjustChildren(nodes : (Node|nestedNode)[]) {

  let x0 = nodes[0].position.x;
  let x1 = nodes[0].position.x + nodes[0].width;
  let y0 = nodes[0].position.y;
  let y1 = nodes[0].position.y + nodes[0].height;

  if(nodes.length > 1) {
    nodes.forEach((node) => {
      if(node.position.x < x0) x0 = node.position.x;
      if(node.position.x + node.width > x1) x1 = node.position.x + node.width;
      if(node.position.y < y0) y0 = node.position.y;
      if(node.position.y + node.height > y1) y1 = node.position.y + node.height;
    })
  }

  let width = x1-x0 + 24;    // linker bzw. oberer Abstand 
  let height = y1-y0 + 42;


  // set basepoint of children to (0,0) -> coordinates will be relative to group node
  let children : Node[] = []
  nodes.forEach((node) => {
    children.push({...node, position: { x: node.position.x-x0+12, y: node.position.y-y0+30} });
  })
  //let rect = getNodesBounds(nodes);
  return { width, height, children};
}

function isNestedNode(node: any): node is nestedNode {
  return node && Array.isArray(node.children);
}

function computeDepth(node: Node | nestedNode, depth: number = 0): number {
  if (!isNestedNode(node) || !node.children || node.children.length === 0) {
    return depth;
  }
  const childDepths = node.children.map(child => computeDepth(child, depth + 1));
  return Math.max(...childDepths);
}

function duplicateProcessComponents(nodes: Node[], groups: hierarchyNode[], edges : Edge[], processes : process[] ) {
  const duplicateNodes: Node[] = [];
    const duplicateEdges: Edge[] = [];
    const duplicateGroups: hierarchyNode[] = [];
    
    processes.forEach((process, rank) => {
        const suffix = `_process_${rank}`;
        const possibleChildNodes = new Set<string>();

        // duplicate every cfg-node
        process.active_nodes.forEach((node) => {
          const original = nodes.find((baseNode) => baseNode.id === node.id) ?? node;
          const nodeId = `${original.id}${suffix}`;

          if(duplicateNodes.find(node => node.id === nodeId)) {
            console.warn(`Duplicate node with id ${nodeId} already exists. Skipping duplication.`);
            return;
          } 

          if(duplicateNodes.includes(node => node.id === nodeId)) return;
          
          const dupParentId = original.parentId ? `${original.parentId}${suffix}` : null;
          const cgi_evaluated_text = original.data?.text ? original.data.text.map((line, idx) => {
            return {
              no: line.no,
              content:  original.data.text[idx].content + " \\\\ " + node.evaluated[idx]
            }
          }) : [];

          const dupNode = {
            ...original,
            id: nodeId,
            data: {
              label: nodeId,
              text: cgi_evaluated_text
            },
            index: nodes.length + duplicateNodes.length,
            position: {
              x: (original.position?.x ?? 0),
              y: (original.position?.y ?? 0),
            },
            parentId: dupParentId,
            hidden: false,
            active: true,
            style: {
              ...(original.style ?? {}),
              opacity: 1,
              filter: "none",
            },
          } as Node;
          duplicateNodes.push(dupNode);
          possibleChildNodes.add(original.id);
        });

        // duplicate function-nodes
        // duplicate every group if there is any child-node active in the process, save in function-nodes in nodes and groups
        groups.forEach((group) => {
          const activeChilds = group.children.filter(child => possibleChildNodes.has(child.id));
          if(activeChilds.length === 0) return;

          // the main group will not be copied, but the child-nodes are added to the original main-group
          if(group.id === "main") {

            const mainChildren: hierarchyNode[] = activeChilds.map(child => {
              const childId = `${child.id}${suffix}`;

              // case A --> child-node is a group thats already duplicated
              const hierarchicalGroup : hierarchyNode = duplicateGroups.find(g => g.id === childId);
              if(hierarchicalGroup) {
                return hierarchicalGroup;
              }
              
              // case B --> child-node is a normal cfg-node --> search node Index in duplicatedNodes
              const childIdx = duplicateNodes.find(node => node.id === childId)?.index;
              if(childIdx !== -1) {
                const hierarchicalChild = {
                  id: childId,
                  index: childIdx,
                  children: [],
                  parentId: null
                } as hierarchyNode;
                return hierarchicalChild;
              }
            });

            // add child-nodes to the orginal main-function-group
            group.children.push(...mainChildren);
            return;
          }

          // other function-groups than main are duplicated and saved in the groups-array as well as the nodes-array ---v

          // find original function-node
          const originalFunctionNode = nodes.find(node => group.id === node.id);
          const dupParentId = originalFunctionNode.parentId !== undefined ? `${originalFunctionNode.parentId}${suffix}` : null;
          const idx = nodes.length + duplicateNodes.length;
          const groupId = `${originalFunctionNode.id}${suffix}`;

          // to be saved in nodes
          const dupNode = {
            ... originalFunctionNode,
            data: {
              label: groupId,
              text: originalFunctionNode.data?.text ?? []
            },
            id: groupId,
            index: idx,
            position: {
              x: (originalFunctionNode.position?.x ?? 0),
              y: (originalFunctionNode.position?.y ?? 0),
            },
            parentId: dupParentId,
            hidden: false,
            active: false
          } as Node;
          duplicateNodes.push(dupNode);
          possibleChildNodes.add(originalFunctionNode.id); // save to check childs for upcoming groups

          // to be saved in groups
          const hierarchicalChildren : hierarchyNode[] = activeChilds.map(child => {
            const childId = `${child.id}${suffix}`;

            // case A --> child is a group thats already duplicated
            const hierarchicalGroup = duplicateGroups.find(g => g.id === childId);
            if(hierarchicalGroup) {
              return hierarchicalGroup;
            }

            // case B --> child is a cfg-node --> search node Index in duplicatedNodes
            const childIdx = duplicateNodes.find(node => node.id === childId)?.index;
            if(childIdx) {
              const hierarchicalChild = {
                id: childId,
                index: childIdx,
                children: [],
                parentId: groupId
              } as hierarchyNode;
              return hierarchicalChild;
            }            
          })

          const dupFunction = {
            id: `${originalFunctionNode.id}${suffix}`,
            index: idx,
            children: [...hierarchicalChildren],
            parentId: group.parentId? `${group.parentId}${suffix}` : null
          } as hierarchyNode;
          duplicateGroups.push(dupFunction);
        })

        // duplicate edges
        process.active_edges.forEach((edge) => {
          // Try to find the original edge by id first --> if not available, match by source/target
          let original = edges.find((baseEdge) => baseEdge.id === edge.id);

          if (!original) {
            console.error(`Original edge not found for edge with id ${edge.id}. Attempting to match by source and target.`);
            return;
          }

          if(duplicateEdges.find(edge => edge.id === `${original.id}${suffix}`)) {
            console.warn(`Duplicate edge with id ${original.id}${suffix} already exists. Skipping duplication.`);
            return;
          } 

          const src = original.source;
          const tar = original.target;

          duplicateEdges.push({
            ...(original ?? edge),
            id: `${original.id}${suffix}`,
            source: `${src}${suffix}`,
            target: `${tar}${suffix}`,
            hidden: false,
          });
        });
    })
    return {duplicateNodes, duplicateGroups, duplicateEdges}
}


const useStore = create<AppState>((set, get) => ({
  cfgFiles: [],
  cgiFiles: [],
  graphData: [],
  selectedLayout: "",
  autoLayout: false,
  selectedMode: "overview",
  setSelectedLayout: (layout: string) => {
    set({ selectedLayout: layout });
  },
  setAutoLayout: (enabled: boolean) => {
    set({ autoLayout: enabled });
  },
  setSelectedMode: (mode: string) => {
    set({ selectedMode: mode });
  },
  loadData: (fileName : string, graphNodes : [], graphEdges : [], fileHandle : FileSystemFileHandle) => {

    if(fileName == undefined || graphNodes.length == 0){
      console.error("Fehler beim Einlesen der Graphdaten von " + fileName + " in den Zustandsspeicher.")
      return -1;
    }

    if(get().cfgFiles.find(file => file.fileName === fileName)) {
      alert("The data file " + fileName + " was already loaded. Please delete or rename the existing file before loading it again.")
      return -1;
    }

    let graphId = get().graphData.length;

      const _fileName : cfgFile = {graphId: graphId, fileName: fileName, fileHandle: fileHandle};

      // lineare Sortierung von Knotendaten und Hierarchie
      const { data, groups } = process_graph_data(graphNodes);

      let nodeIds = data.map((nd) => nd.id);
      let edges : Edge[] = [];
      graphEdges.forEach((edg : Edge) => {
        if(nodeIds.includes(edg.source) && nodeIds.includes(edg.target)) {
          edges.push(edg);
        }
      })

      const graphData : graphData = {
        graphId : graphId,
        nodes : data,
        groups : groups,
        edges : edges, 
        processes : [],
        duplicates_calculated: false
      }

      let cfgData_arr = get().graphData.concat(graphData);
      let cfgFile_arr = get().cfgFiles.concat(_fileName);

    set({
      cfgFiles : cfgFile_arr,
      graphData: cfgData_arr
    }) 
  },
  addProcessInformation: (fileName : string, processes : process[], fileHandle : FileSystemFileHandle) => {

    // 1. search in cfgFile_arr for filename (program name)
    let cfg_index = get().cfgFiles.findIndex(file => (file.fileName.slice(0, file.fileName.length-9) === fileName.slice(0, fileName.length-9)));

    // 2. duplicate nodes, edges and groups for process-visualization
    const graphDataEntry = get().graphData[cfg_index];
    const { duplicateNodes, duplicateGroups, duplicateEdges } = duplicateProcessComponents(graphDataEntry.nodes, graphDataEntry.groups, graphDataEntry.edges, processes);

    // 3. add cgi data to corresponding graphData entry
    let _graphData = get().graphData;
    let _graphDataEntry = _graphData[cfg_index];

    const newGraphDataEntry = {
      ..._graphDataEntry,
      processes: processes,
      nodes: [...graphDataEntry.nodes, ...duplicateNodes],
      groups: [...graphDataEntry.groups.filter((group) => group.id !== "main"), ...duplicateGroups, graphDataEntry.groups.find((group) => group.id === "main")],
      edges: [...graphDataEntry.edges, ...duplicateEdges]
    };

    const cgiFileEntry : cfgFile = {
      graphId: cfg_index,
      fileName : fileName,
      fileHandle: fileHandle
    }

    const cgiFiles = [...get().cgiFiles];
    cgiFiles[cfg_index] = cgiFileEntry;

    set({
      graphData: [..._graphData.slice(0, cfg_index), newGraphDataEntry, ..._graphData.slice(cfg_index + 1)],
      cgiFiles
    }) 
  },
  showProcessNodes: (graphId: number, selectedMode: string) => {

    const isProcessDuplicate = (id: string) => /_process_\d+$/.test(id);
    let activeNodeIds: string[] = [];
    let activeEdgeIds: string[] = [];
    let activeGroupIds: string[] = [];
    const graphDataEntry = get().graphData[graphId];

    if (graphDataEntry.processes.length === 0) {
      window.alert("No process information loaded for graph " + graphId + "! Please load the corresponding .cgi-file.");
      return;
    }

    // 1. mode: overview (normal CFG)
    if (selectedMode === "overview") {
      activeNodeIds = graphDataEntry.nodes.filter((node) => !isProcessDuplicate(node.id)).map((node) => node.id);
      activeEdgeIds = graphDataEntry.edges.filter((edge) => !isProcessDuplicate(edge.id)).map((edge) => edge.id);
      activeGroupIds = graphDataEntry.groups.filter((group) => !isProcessDuplicate(group.id)).map((group) => group.id);
    } 

    // 2. mode: side-by-side (all processes are visualized next to each other)
    else if (selectedMode === "side-by-side") {
      activeNodeIds = graphDataEntry.nodes.filter((node) => isProcessDuplicate(node.id)).map((node) => node.id);
      activeEdgeIds = graphDataEntry.edges.filter((edge) => isProcessDuplicate(edge.id)).map((edge) => edge.id);
      activeGroupIds = graphDataEntry.groups.filter((group) => isProcessDuplicate(group.id)).map((group) => group.id);
    }

    // 3. mode: RankX (only the selected process is visualized) 
    else { 
      const selectedProcess = graphDataEntry.processes.find((process) => process.mpi_rank === Number(selectedMode));
      if (!selectedProcess) {
        console.error(`Process with rank ${selectedMode} not found.`);
        return;
      }
      const suffix = `_process_${selectedProcess.mpi_rank}`;
      activeNodeIds = graphDataEntry.nodes.filter((node) => isProcessDuplicate(node.id) && node.id.endsWith(suffix)).map((node) => node.id);
      activeEdgeIds = graphDataEntry.edges.filter((edge) => isProcessDuplicate(edge.id) && edge.id.endsWith(suffix)).map((edge) => edge.id);
      activeGroupIds = graphDataEntry.groups.filter((group) => isProcessDuplicate(group.id) && group.id.endsWith(suffix)).map((group) => group.id);
    }

    const activeNodes = graphDataEntry.nodes.map((node) => ({
      ...node,
      hidden: !activeNodeIds.includes(node.id),
      active: activeNodeIds.includes(node.id)
    }));

    const activeEdges = graphDataEntry.edges.map((edge) => ({
      ...edge,
      hidden: !activeEdgeIds.includes(edge.id),
    }));

    const activeGroups = graphDataEntry.groups.map((group) => {
      if(group.id === "main") {
        return {
          ...group,
          hidden: false,
          active: true
        }
      }
      return {
      ...group,
      hidden: !activeGroupIds.includes(group.id),
      active: activeGroupIds.includes(group.id)
    }
    });

    set({
      graphData: get().graphData.map((data) => {
        if (data.graphId !== graphId) {
          return data;
        }
        return { ...data, nodes: activeNodes, edges: activeEdges, groups: activeGroups};
      }),
    });
    
  },
  updateData: (graphId: number, graphNodes : Node[], graphEdges : Edge[], graphProcesses : process[]) => {
  
  const newNodes = graphNodes.map(node => ({
    ...node,
    type: node.type || 'cfg-node',
    position: node.position || { x: 0, y: 0 },
    data: {
      label: node.data?.label || '',
      text: node.data?.text || [],
    },
  }));

  let { data, groups } = process_graph_data(newNodes);
  let newEdges = graphEdges.map(e => ({
    ...e,
    source: e.source.toString(),
    target: e.target.toString(),
  }));

  // Add updated process information
  const { duplicateNodes, duplicateGroups, duplicateEdges } = duplicateProcessComponents(data, groups, newEdges, graphProcesses);

  data = [...data, ...duplicateNodes];
  groups = [...groups.filter((group) => group.id !== "main"), ...duplicateGroups, groups.find((group) => group.id === "main")];
  newEdges = [...newEdges, ...duplicateEdges];

 set({
    graphData: get().graphData.map(graph => {
        if(graph.graphId === graphId) {
          return { ...graph, nodes: data, groups: groups, edges: newEdges, processes: graphProcesses}
        }
        return graph;
      })
  });
  },
  rename: (graphId : number, fileName : string ) => {
    const cfgFile_arr = get().cfgFiles.map(file =>
      file.graphId === Number(graphId) ? { ...file, fileName } : file
    );
    set({
      cfgFiles: cfgFile_arr
    });
  },
  removeData: (graphId : number) => {
    let cfgFile_arr = get().cfgFiles.filter(file => file.graphId !== graphId);
    let graphData_arr = get().graphData.filter(data => data.graphId !== graphId);

    cfgFile_arr = cfgFile_arr.map((file, index) => {
      file.graphId = index;
      return file;
    });
    graphData_arr = graphData_arr.map((data, index) => {
      data.graphId = index;
      return data;
    });
    set({
      cfgFiles: cfgFile_arr,
      graphData: graphData_arr
    });
  },
  setNodes: (graphId: number, nodes: (Node | nestedNode)[]) => {

  const styledNodes = nodes.map(n => {
    return {
      ...n,
      style: {
        ...n.style,
        zIndex: n.type === "function-node" && isNestedNode(n)
          ? 1000 - computeDepth(n)
          : 2000 // normale Knoten ganz oben
      }
    };
  });

    set({
      graphData: get().graphData.map((data) => {
        if(data.graphId === graphId) {
          return { ...data, nodes: styledNodes}
        }
        return data;
      })
    })
  },
  mergeNodes: (graphId : number, layoutedNodes: Node[]) => {
    const mergedNodes : Node[] = [];
    const ids_of_layoutedNodes = layoutedNodes.map(node => node.id)
    get().graphData[graphId].nodes.forEach((node) => {
        if(ids_of_layoutedNodes.includes(node.id)) {        // find()
            const index = ids_of_layoutedNodes.indexOf(node.id);
            const layoutedNode = layoutedNodes[index];
            mergedNodes.push({...node, position: layoutedNode.position, width: layoutedNode.width, height: layoutedNode.height});
        }
        else {
            mergedNodes.push(node)
        }
    })
    return mergedNodes;
  },
  onNodesChange: (changes: NodeChange[]) => {
    const graphId = Number(window.location.pathname.replace("/graphs/",""));
    const changed_nodes = applyNodeChanges(changes, get().graphData[graphId].nodes)
  
    set({
      graphData: get().graphData.map((data) => {
        if(data.graphId === graphId) {
          const new_data : graphData = {
            graphId: data.graphId,
            nodes: changed_nodes,
            groups: data.groups,
            edges: data.edges,
            processes: data.processes,
            duplicates_calculated: data.duplicates_calculated
          }
          return new_data;
        }
        return data;
      })
    })
  },
  updateNodeState: (graphId : number, nodeId : string, state: boolean) => {
    const graphData = get().graphData[graphId];
    const _nodes : Node[] = graphData.nodes.map((node : (Node)) => {
      if(node.id === nodeId) {
        node.hidden = state;
      }
      return node; 
    })

    get().setNodes(graphId, _nodes);
  },
  getBoundingBoxAndAdjustedChildren : (graphId : number, layoutedNodes : (Node|nestedNode)[], functionDimensions?: functionDimension[]) => {

    // Return early if no visible nodes in this group
    if(layoutedNodes.length === 0) {
      return [];
    }

    const functionNodeId = (layoutedNodes[0].parentId? layoutedNodes[0].parentId : undefined);

    if(functionNodeId !== undefined) { 
      const { width, height, children } = calculateBoundingBoxAndAdjustChildren(layoutedNodes);

      if(functionDimensions !== undefined) {
        // save dimensions of function nodes because ELK resets dimensions while layouting 
        let nd = functionDimensions.find(node => node.id === functionNodeId);
        if(nd !== undefined) {
          nd.width = width;
          nd.height = height;
        }
        else {
          let entry : functionDimension = {
            id: functionNodeId,
            width: width,
            height: height
          }
          functionDimensions.push(entry)
        }
      }

      // find parentNode in data, set its width/height and add to children
      let parentNode = get().graphData[graphId].nodes.find((node) => node.id === functionNodeId);
      if(parentNode !== undefined) {
        parentNode = {...parentNode, width: width, height: height};
        children.push(parentNode)
      }
      else { console.error("ParentNode " + functionNodeId + " not found in graphs data. Couldn't add to children.") }

      return children;
    }
    return [];
  },
  getGroupEdges: (graphId : number, groupNodes : Node[], edges: Edge[]) => {
    const nodes = get().graphData[graphId].nodes;
    const groupNodesIds = groupNodes.map(node => node.id);
    
    let edgesForLayouting : Edge[] = [];
    edges.forEach((edge) => {
      if(groupNodesIds.includes(edge.source) && groupNodesIds.includes(edge.target)) {
        edgesForLayouting.push(edge);
      }
      else {
        if(!(!groupNodesIds.includes(edge.source) && !groupNodesIds.includes(edge.target))) {
          let src : string = edge.source;
          let tar : string = edge.target;

          while((!groupNodesIds.includes(src)) && src !== "") {
            src = nodes.find(node => node.id === src)?.parentId ?? "";
          }

          while((!groupNodesIds.includes(tar)) && tar !== ""){
            tar = nodes.find(node => node.id === tar)?.parentId ?? "";
          }

          if(!(src === "" || tar === "")) {
            let edg : Edge = {
              id: "layout-f-"+src+"-t-"+tar,
              source: src,
              target: tar
            }
            edgesForLayouting.push(edg);
          }
        }
      }
    })
    return edgesForLayouting;
  },
  addBridgeEdge: (graphId : number, edgeToAdd: Edge) => {
    set({
      graphData: get().graphData.map((data) => { 
        if(data.graphId === graphId) {
          let _edges : Edge[] = []
          get().graphData[graphId].edges.forEach((edge) => {
            if(!edge.id.includes("bridge")) {             // normale Kanten direkt kopieren
              _edges.push(edge)
            }
            else {
              let sourceNode = get().graphData[graphId].nodes.find((node) => node.id === edge.source);
              let targetNode = get().graphData[graphId].nodes.find((node) => node.id === edge.target);

              if(sourceNode === undefined || targetNode === undefined) console.error("Source oder Target nicht gefunden von Kante " + edge.id + ".")

              if(sourceNode?.hidden === false && targetNode?.hidden === false) {      // delete bridgeEdges of subflows inside functionNode
                _edges.push(edge)
              }
            }
          })
          _edges = addEdge(edgeToAdd, _edges);
          return { ...data, edges: _edges }
        }
        return data;
      })
    })
  },
  deleteBridgeEdge: (graphId : number, edgeToDelete : Edge) => {
    set({
      graphData: get().graphData.map((data) => {
        if(data.graphId === graphId) {
          const _edges = data.edges.filter((edge) => edge.id !== edgeToDelete.id);
          return { ...data, edges: _edges }
        }
        return data;
      })
    })
  }
}));

export default useStore;