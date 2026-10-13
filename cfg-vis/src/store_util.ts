import { Node, Edge }  from "reactflow";

export type cfgFile = {
    graphId : number, 
    fileName : string,
    fileHandle : FileSystemFileHandle
}

export type nestedNode = Node & {children: (Node | nestedNode)[]}
export type cloneNode = Node & {children?: (Node | cloneNode)[], parentId?: string, extent?: string};

export type hierarchyNode = {
    id : string,
    index : number,                 // Position des Knotens im Datenarray 
    children : hierarchyNode[],
    parentId : string | null
}

export type process = {
    mpi_rank: number,
    active_nodes : Node[],
    active_edges : Edge[]
}

export type graphData = {
    graphId: number,
    nodes : Node[],
    groups : hierarchyNode[],
    edges : Edge[],
    processes : process[],
    duplicates_calculated: boolean
}

export type functionDimension = {
    id: string,
    width: number,
    height: number
  }