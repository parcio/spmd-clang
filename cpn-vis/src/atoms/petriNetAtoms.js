import { atom } from 'jotai';
import stateGraph from '../data/sg_reactflow.json';
import petriNet from '../data/net_reactflow.json';


export const petriNetDataAtom = atom(petriNet);
export const stateGraphDataAtom = atom(stateGraph);
export const petriNetNodesAtom = atom([]);
export const petriNetEdgesAtom = atom([]);
export const petriNetLoadingAtom = atom(true);
export const elkAvailableAtom = atom(true);
export const autoLayoutAtom = atom(true);

export const currentStateIdAtom = atom(stateGraph.metadata?.initialState || "0");
export const historyAtom = atom([stateGraph.metadata?.initialState || "0"]);
export const selectedPathsAtom = atom({});
export const firingTransitionAtom = atom(null);
export const fireAnimationAtom = atom(false);
export const lastClickedTransitionAtom = atom(null);

export const showClickHelpAtom = atom(false);
export const clickableTransitionsAtom = atom(new Set());


export const currentStateAtom = atom(
    (get) => {
        const stateId = get(currentStateIdAtom);
        const stateGraph = get(stateGraphDataAtom);
        return stateGraph.states.find(s => s.id === stateId) || stateGraph.states[0];
    }
);

export const transitionMapAtom = atom(
    (get) => {
        const stateGraph = get(stateGraphDataAtom);
        const transitionMap = {};

        stateGraph.edges.forEach(edge => {
            const sourceState = edge.source;
            const transition = edge.label;

            if (!transitionMap[sourceState]) {
                transitionMap[sourceState] = {};
            }
            transitionMap[sourceState][transition] = edge.target;
        });

        return transitionMap;
    }
);

export const availableTransitionsAtom = atom(
    (get) => {
        const stateId = get(currentStateIdAtom);
        const transitionMap = get(transitionMapAtom);

        if (!stateId || !transitionMap[stateId]) return [];

        return Object.keys(transitionMap[stateId]).map(transitionId => ({
            transitionId,
            targetStateId: transitionMap[stateId][transitionId]
        }));
    }
);

export const isAtBranchingPointAtom = atom(
    (get) => {
        const stateId = get(currentStateIdAtom);
        const stateGraph = get(stateGraphDataAtom);
        return stateGraph.metadata?.branchingPoints?.includes(stateId) || false;
    }
);

export const isAtFinalStateAtom = atom(
    (get) => {
        const stateId = get(currentStateIdAtom);
        const stateGraph = get(stateGraphDataAtom);
        return stateId === stateGraph.metadata?.finalState;
    }
);


export const petriNetNodesWithRuntimeAtom = atom(
    (get) => {
        const nodes = get(petriNetNodesAtom);
        const currentState = get(currentStateAtom);
        const availableTransitions = get(availableTransitionsAtom);
        const fireAnimation = get(fireAnimationAtom);
        const firingTransition = get(firingTransitionAtom);
        const lastClickedTransition = get(lastClickedTransitionAtom);

        if (!currentState || !nodes.length) return nodes;

        return nodes.map(node => {
            if (node.data.type === 'place') {

                const tokens = currentState.marking[node.id] || [];

                return {
                    ...node,
                    data: {
                        ...node.data,
                        tokens: tokens,
                        isActive: tokens.length > 0,
                    }
                };
            } else if (node.data.type === 'transition') {
                const isAvailable = availableTransitions.some(t => t.transitionId === node.id);
                const isFiring = fireAnimation && node.id === firingTransition;
                const wasJustClicked = lastClickedTransition === node.id;

                return {
                    ...node,
                    data: {
                        ...node.data,
                        isEnabled: isAvailable,
                        isFirable: isAvailable,
                        isFiring: isFiring,
                        isClickable: isAvailable && !fireAnimation,
                        isAvailable: isAvailable,
                        wasJustClicked: wasJustClicked,
                        tokens: [],
                    }
                };
            }
            return node;
        });
    }
);


export const petriNetEdgesWithRuntimeAtom = atom(
    (get) => {
        const edges = get(petriNetEdgesAtom);
        const firingTransition = get(firingTransitionAtom);

        return edges.map(edge => {
            const isFiringEdge = firingTransition &&
                (edge.target === firingTransition || edge.source === firingTransition);

            return {
                ...edge,
                type: 'smoothstep',
                animated: isFiringEdge,
                style: {
                    stroke: isFiringEdge ? '#ff9800' : '#b1b1b7',
                    strokeWidth: isFiringEdge ? 3 : 2,
                    opacity: isFiringEdge ? 1 : 0.8,
                    transition: 'all 0.3s ease'
                },
                markerEnd: {
                    type: 'arrowclosed',
                    color: isFiringEdge ? '#ff9800' : '#b1b1b7',
                    width: isFiringEdge ? 20 : 15,
                    height: isFiringEdge ? 20 : 15,
                }
            };
        });
    }
);


export const petriNetWithRuntimeAtom = atom(
    (get) => {
        const nodes = get(petriNetNodesWithRuntimeAtom);
        const edges = get(petriNetEdgesWithRuntimeAtom);

        return { nodes, edges };
    }
);

export const placeTokensAtom = (placeId) => atom(
    (get) => {
        const currentState = get(currentStateAtom);
        return currentState?.marking[placeId] || [];
    }
);