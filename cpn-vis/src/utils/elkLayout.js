import { MarkerType } from 'reactflow';
import { parseHtmlLabel } from './htmlParser';

let elkPromise = null;

const getELK = async () => {
    if (!elkPromise) {
        elkPromise = import('elkjs/lib/elk.bundled.js').then(module => {
            const ELK = module.default;
            return new ELK();
        });
    }
    return elkPromise;
};

const EXCLUDED_NODES = ['Buffer', 'Signals'];

const isExcluded = (nodeId) => {
    return EXCLUDED_NODES.some(name => nodeId.includes(name));
};

export const createELKGraph = (nodes, edges) => {
    const filteredNodes = nodes.filter(node => !isExcluded(node.id));
    const allowedNodeIds = new Set(filteredNodes.map(node => node.id));
    const filteredEdges = edges.filter(edge =>
        allowedNodeIds.has(edge.source) && allowedNodeIds.has(edge.target)
    );

    return {
        id: "root",
        layoutOptions: {
            'elk.algorithm': 'layered',
            'elk.direction': 'RIGHT',
            'elk.layered.spacing.nodeNodeBetweenLayers': '150',
            'elk.spacing.nodeNode': '120',
            'elk.layered.nodePlacement.strategy': 'NETWORK_SIMPLEX',
            'elk.layered.crossingMinimization.strategy': 'LAYER_SWEEP',
            'elk.layered.considerModelOrder.strategy': 'NODES_AND_EDGES',
            'elk.layered.cycleBreaking.strategy': 'DEPTH_FIRST',
            'elk.layered.layering.strategy': 'NETWORK_SIMPLEX',
            'elk.layered.mergeEdges': 'true',
            'elk.layered.feedbackEdges': 'true',
            'elk.edgeRouting': 'SPLINES',
            'elk.contentAlignment': 'H_CENTER',
            'elk.separateConnectedComponents': 'false',
            'elk.padding': '[top=50, left=50, bottom=50, right=50]',
        },
        children: filteredNodes.map(node => ({
            id: node.id,
            width: node.data.type === 'place' ? 100 : 140,
            height: node.data.type === 'place' ? 100 : 80,
        })),
        edges: filteredEdges.map(edge => ({
            id: edge.id,
            sources: [edge.source],
            targets: [edge.target],
        }))
    };
};

export const applyELKLayout = async (nodes, edges) => {
    console.log('Applying ELK.js layout...');

    try {
        const elk = await getELK();
        const elkGraph = createELKGraph(nodes, edges);
        const layout = await elk.layout(elkGraph);

        if (layout && layout.children) {
            const positionedNodes = nodes.map(node => {
                const elkNode = layout.children.find(n => n.id === node.id);
                const position = elkNode ? { x: elkNode.x, y: elkNode.y } : node.position;

                return {
                    ...node,
                    position,
                    data: {
                        ...node.data,
                        isClickable: false,
                        parsedLabel: parseHtmlLabel(node.data.htmlLabel) || node.data.label,
                        rawHtmlLabel: node.data.htmlLabel
                    }
                };
            });

            const allowedNodeIds = new Set(nodes.filter(n => !isExcluded(n.id)).map(n => n.id));
            const positionedEdges = edges
                .filter(edge => allowedNodeIds.has(edge.source) && allowedNodeIds.has(edge.target))
                .map(edge => ({
                    ...edge,
                    markerEnd: {
                        type: MarkerType.ArrowClosed,
                        color: '#b1b1b7',
                        width: 15,
                        height: 15,
                    }
                }));

            console.log('ELK layout applied successfully');
            return { nodes: positionedNodes, edges: positionedEdges, success: true };
        }
    } catch (error) {
        console.error('ELK layout failed:', error);
        return { success: false };
    }
};

export const applyManualLayout = (nodes, edges) => {
    console.log('Applying manual layout...');

    const allowedNodeIds = new Set(nodes.filter(n => !isExcluded(n.id)).map(n => n.id));
    const filteredEdges = edges.filter(edge =>
        allowedNodeIds.has(edge.source) && allowedNodeIds.has(edge.target)
    );

    const positionedNodes = nodes.map(node => ({
        ...node,
        data: {
            ...node.data,
            isClickable: false,
            parsedLabel: parseHtmlLabel(node.data.htmlLabel) || node.data.label,
            rawHtmlLabel: node.data.htmlLabel
        }
    }));

    const positionedEdges = filteredEdges.map(edge => ({
        ...edge,
        markerEnd: {
            type: MarkerType.ArrowClosed,
            color: '#b1b1b7',
            width: 15,
            height: 15,
        }
    }));

    return { nodes: positionedNodes, edges: positionedEdges };
};