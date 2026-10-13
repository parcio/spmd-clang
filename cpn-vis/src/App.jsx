import React, {useCallback, useEffect, useMemo } from 'react';
import ReactFlow, {
    Controls,
    Background,
    applyNodeChanges,
    applyEdgeChanges,
    addEdge,
    ReactFlowProvider,
    MarkerType,
} from 'reactflow';
import { useAtom, useAtomValue, useSetAtom } from 'jotai';
import 'reactflow/dist/style.css';

import ControlPanel from './components/ControlPanel';
import StatusPanel from './components/StatusPanel';
import StatusIndicator from './components/StatusIndicator';
import ClickHelpMessage from './components/ClickHelpMessage';
import LoadingSpinner from './components/LoadingSpinner';
import PetriNetNode from './components/PetriNetNode.jsx';

import {
    petriNetNodesAtom,
    petriNetEdgesAtom,
    petriNetLoadingAtom,
    elkAvailableAtom,
    autoLayoutAtom,
    currentStateIdAtom,
    historyAtom,
    selectedPathsAtom,
    firingTransitionAtom,
    fireAnimationAtom,
    lastClickedTransitionAtom,
    transitionMapAtom,
    availableTransitionsAtom,
    showClickHelpAtom,
    clickableTransitionsAtom,
    currentStateAtom,
    isAtBranchingPointAtom,
    isAtFinalStateAtom
} from './atoms/petriNetAtoms';


import { applyELKLayout, applyManualLayout } from './utils/elkLayout';
import { getTargetState } from './utils/targetState.js';
import { useKeyboardShortcuts } from './hooks/useKeyboardShortcuts';
import stateGraph from './data/sg_reactflow.json';
import petriNet from './data/net_reactflow.json';

const nodeTypes = {
    petriPlace: PetriNetNode,
    petriTransition: PetriNetNode,
};

function AppContent() {

    const [nodes, setNodes] = useAtom(petriNetNodesAtom);
    const [edges, setEdges] = useAtom(petriNetEdgesAtom);
    const [loading, setLoading] = useAtom(petriNetLoadingAtom);
    const [elkAvailable, setElkAvailable] = useAtom(elkAvailableAtom);
    const [autoLayout, setAutoLayout] = useAtom(autoLayoutAtom);

    const [currentStateId, setCurrentStateId] = useAtom(currentStateIdAtom);
    const [history, setHistory] = useAtom(historyAtom);
    const [selectedPaths, setSelectedPaths] = useAtom(selectedPathsAtom);
    const [firingTransition, setFiringTransition] = useAtom(firingTransitionAtom);
    const [fireAnimation, setFireAnimation] = useAtom(fireAnimationAtom);
    const [lastClickedTransition, setLastClickedTransition] = useAtom(lastClickedTransitionAtom);

    const transitionMap = useAtomValue(transitionMapAtom);
    const availableTransitions = useAtomValue(availableTransitionsAtom);
    const currentState = useAtomValue(currentStateAtom);
    const isAtBranchingPoint = useAtomValue(isAtBranchingPointAtom);
    const isAtFinalState = useAtomValue(isAtFinalStateAtom);

    const showClickHelp = useAtomValue(showClickHelpAtom);
    const setShowClickHelp = useSetAtom(showClickHelpAtom);
    const setClickableTransitions = useSetAtom(clickableTransitionsAtom);


    useEffect(() => {
        const initializeData = async () => {
            const result = await applyELKLayout(petriNet.nodes, petriNet.edges);

            if (result && result.success) {
                setNodes(result.nodes);
                setEdges(result.edges);
                setElkAvailable(true);
            } else {
                const fallback = applyManualLayout(petriNet.nodes, petriNet.edges);
                setNodes(fallback.nodes);
                setEdges(fallback.edges);
                setElkAvailable(false);
            }

            setLoading(false);
        };

        initializeData();
    }, [setNodes, setEdges, setLoading, setElkAvailable]);


    useEffect(() => {
        const newClickableSet = new Set(availableTransitions.map(t => t.transitionId));
        setClickableTransitions(newClickableSet);
    }, [availableTransitions, setClickableTransitions]);

    const handleTransitionClick = useCallback((transitionId) => {

        if (fireAnimation || firingTransition) {
            return;
        }

        const targetStateId = getTargetState(transitionMap, currentStateId, transitionId);

        if (!targetStateId) {
            setShowClickHelp(true);
            setTimeout(() => setShowClickHelp(false), 2000);
            return;
        }

        setLastClickedTransition(transitionId);
        setFiringTransition(transitionId);
        setFireAnimation(true);

        if (stateGraph.metadata?.branchingPoints?.includes(currentStateId)) {
            setSelectedPaths(prev => ({
                ...prev,
                [currentStateId]: targetStateId
            }));
        }

        setTimeout(() => {
            setCurrentStateId(targetStateId);
            setHistory(prev => [...prev, targetStateId]);

            setTimeout(() => {
                setFireAnimation(false);
                setFiringTransition(null);
                setLastClickedTransition(null);
            }, 300);
        }, 600);
    }, [currentStateId, fireAnimation, firingTransition, transitionMap, setShowClickHelp, setSelectedPaths, setCurrentStateId, setHistory]);


    const handleNodeClick = useCallback((event, node) => {
        if (node.data.type !== 'transition') {
            return;
        }

        if (!node.data.isClickable) {
            setShowClickHelp(true);
            setTimeout(() => setShowClickHelp(false), 2000);
            return;
        }

        const isCtrlPressed = event.ctrlKey || event.metaKey;

        if (isCtrlPressed) {
            event.preventDefault();
            event.stopPropagation();
            handleTransitionClick(node.id);
        } else {
            setShowClickHelp(true);
            setTimeout(() => setShowClickHelp(false), 2000);
        }
    }, [handleTransitionClick, setShowClickHelp]);


    const handleEdgeClick = useCallback(() => {
        setShowClickHelp(true);
        setTimeout(() => setShowClickHelp(false), 2000);
    }, [setShowClickHelp]);

    const handleNext = useCallback(() => {
        if (availableTransitions.length === 0 || fireAnimation) return;

        if (availableTransitions.length > 1) {
            setShowClickHelp(true);
            setTimeout(() => setShowClickHelp(false), 3000);
        } else {
            const transitionId = availableTransitions[0].transitionId;
            handleTransitionClick(transitionId);
        }
    }, [availableTransitions, fireAnimation, handleTransitionClick, setShowClickHelp]);


    const handlePrevious = useCallback(() => {
        if (history.length > 1 && !fireAnimation) {
            const newHistory = [...history];
            newHistory.pop();
            const prevState = newHistory[newHistory.length - 1];

            setHistory(newHistory);
            setCurrentStateId(prevState);

            if (stateGraph.metadata?.branchingPoints?.includes(prevState)) {
                setSelectedPaths(prev => {
                    const newPaths = { ...prev };
                    delete newPaths[prevState];
                    return newPaths;
                });
            }
        }
    }, [history, fireAnimation, setCurrentStateId, setHistory, setSelectedPaths]);

    const handleReset = useCallback(() => {
        setCurrentStateId(stateGraph.metadata?.initialState || "0");
        setSelectedPaths({});
        setShowClickHelp(false);
        setHistory([stateGraph.metadata?.initialState || "0"]);
        setFiringTransition(null);
        setFireAnimation(false);
        setLastClickedTransition(null);

        if (autoLayout) {
            setTimeout(async () => {
                const result = await applyELKLayout(petriNet.nodes, petriNet.edges);
                if (result && result.success) {
                    setNodes(result.nodes);
                    setEdges(result.edges);
                    setElkAvailable(true);
                }
            }, 100);
        }
    }, [autoLayout, setCurrentStateId, setHistory, setSelectedPaths, setShowClickHelp, setNodes, setEdges, setElkAvailable]);


    const handleReapplyLayout = useCallback(async () => {
        setLoading(true);
        const result = await applyELKLayout(petriNet.nodes, petriNet.edges);
        if (result && result.success) {
            setNodes(result.nodes);
            setEdges(result.edges);
            setElkAvailable(true);
        } else {
            const fallback = applyManualLayout(petriNet.nodes, petriNet.edges);
            setNodes(fallback.nodes);
            setEdges(fallback.edges);
            setElkAvailable(false);
        }
        setLoading(false);
    }, [setNodes, setEdges, setLoading, setElkAvailable]);


    const onNodesChange = useCallback(
        (changes) => {
            setNodes((nds) => applyNodeChanges(changes, nds));
        },
        [setNodes]
    );

    const onEdgesChange = useCallback(
        (changes) => {
            setEdges((eds) => applyEdgeChanges(changes, eds));
        },
        [setEdges]
    );

    const onConnect = useCallback(
        (params) => {
            setEdges((eds) => addEdge(params, eds));
        },
        [setEdges]
    );

    useKeyboardShortcuts(handleNext, fireAnimation);

    const styledNodes = useMemo(() => {
        if (!currentState || !nodes.length) return nodes;

        return nodes.map(node => {
            if (node.data.type === 'place') {
                const tokens = currentState.marking[node.id] || [];
                const isActive = tokens.length > 0;

                const htmlLabel = `<strong>${node.data.id}</strong><br/>${
                    tokens.length > 0 ? tokens.join('<br/>') + '<br/>' : ''
                }`;

                return {
                    ...node,
                    data: {
                        ...node.data,
                        tokens: tokens,
                        isActive: isActive,
                        parsedLabel: htmlLabel,
                        label: htmlLabel
                    }
                };
            } else if (node.data.type === 'transition') {
                const isAvailable = availableTransitions.some(t => t.transitionId === node.id);
                const isFiring = fireAnimation && node.id === firingTransition;
                const wasJustClicked = lastClickedTransition === node.id;

                let backgroundColor, borderColor, textColor, fontWeight, boxShadow, borderWidth, cursorStyle;

                if (isFiring) {
                    backgroundColor = '#fff3e0';
                    borderColor = '#ff9800';
                    textColor = '#e65100';
                    fontWeight = 'bold';
                    boxShadow = '0 0 20px rgba(255, 152, 0, 0.9)';
                    borderWidth = '3px';
                    cursorStyle = 'wait';
                } else if (isAvailable) {
                    backgroundColor = '#bbdefb';
                    borderColor = '#2196f3';
                    textColor = '#0d47a1';
                    fontWeight = 'bold';
                    boxShadow = '0 0 12px rgba(33, 150, 243, 0.7)';
                    borderWidth = '3px';
                    cursorStyle = 'pointer';
                } else {
                    backgroundColor = '#f5f5f5';
                    borderColor = '#9e9e9e';
                    textColor = '#616161';
                    fontWeight = 'normal';
                    boxShadow = 'none';
                    borderWidth = '2px';
                    cursorStyle = 'not-allowed';
                }

                if (wasJustClicked && !isFiring) {
                    backgroundColor = '#e3f2fd';
                    borderColor = '#1976d2';
                    cursorStyle = 'wait';
                }

                return {
                    ...node,
                    data: {
                        ...node.data,
                        isEnabled: isAvailable,
                        isFiring: isFiring,
                        isClickable: isAvailable && !fireAnimation,
                        isAvailable: isAvailable,
                        parsedLabel: node.data.parsedLabel,
                        label: node.data.parsedLabel
                    },
                    style: {
                        cursor: cursorStyle,
                        filter: 'none',
                        transition: 'all 0.3s ease',
                        backgroundColor: backgroundColor,
                        borderColor: borderColor,
                        borderWidth: borderWidth,
                        borderStyle: 'solid',
                        color: textColor,
                        fontWeight: fontWeight,
                        boxShadow: boxShadow,
                        opacity: (isAvailable || isFiring) ? 1 : 0.6
                    }
                };
            }
            return node;
        });
    }, [nodes, currentState, availableTransitions, fireAnimation, firingTransition, lastClickedTransition]);

    const styledEdges = useMemo(() => {
        return edges.map(edge => {
            const isFiringEdge = firingTransition &&
                (edge.target === firingTransition || edge.source === firingTransition);

            return {
                ...edge,
                type: 'smoothstep',
                animated: isFiringEdge,
                style: {
                    stroke: isFiringEdge ? '#ff9800' : '#888',
                    strokeWidth: isFiringEdge ? 3 : 1.5,
                    opacity: isFiringEdge ? 1 : 0.6,
                    transition: 'all 0.3s ease'
                },
                markerEnd: {
                    type: MarkerType.ArrowClosed,
                    color: isFiringEdge ? '#ff9800' : '#888',
                    width: isFiringEdge ? 20 : 15,
                    height: isFiringEdge ? 20 : 15,
                }
            };
        });
    }, [edges, firingTransition]);

    if (loading) {
        return <LoadingSpinner />;
    }

    return (
        <div style={{
            width: '100vw',
            height: '100vh',
            display: 'flex',
            flexDirection: 'column',
            background: '#f7fafc',
            overflow: 'hidden'
        }}>
            <ControlPanel
                onNext={handleNext}
                onPrevious={handlePrevious}
                onReset={handleReset}
                onReapplyLayout={handleReapplyLayout}
            />

            <div style={{
                flex: 1,
                display: 'flex',
                flexDirection: 'column',
                minHeight: 0,
                position: 'relative'
            }}>
                <StatusIndicator />
                {showClickHelp && <ClickHelpMessage />}

                <div style={{
                    flex: 1,
                    position: 'relative',
                    background: '#fafafa',
                    minHeight: 0
                }}>
                    <ReactFlow
                        nodes={styledNodes}
                        edges={styledEdges}
                        nodeTypes={nodeTypes}
                        onNodesChange={onNodesChange}
                        onEdgesChange={onEdgesChange}
                        onConnect={onConnect}
                        onNodeClick={handleNodeClick}
                        onEdgeClick={handleEdgeClick}
                        fitView={autoLayout}
                        fitViewOptions={{
                            padding: 0.4,
                            minZoom: 0.5,
                            maxZoom: 1.5
                        }}
                        nodesDraggable={true}
                        nodesConnectable={false}
                        elementsSelectable={true}
                        proOptions={{ hideAttribution: true }}
                        defaultEdgeOptions={{
                            type: 'smoothstep',
                            style: { strokeWidth: 2 },
                            markerEnd: { type: MarkerType.ArrowClosed }
                        }}
                        selectNodesOnDrag={false}
                        panOnDrag={[1, 2]}
                        panOnScroll={true}
                        zoomOnScroll={true}
                        zoomOnPinch={true}
                        zoomOnDoubleClick={true}
                        minZoom={0.2}
                        maxZoom={2}
                    >
                        <Controls
                            style={{
                                boxShadow: '0 2px 8px rgba(0,0,0,0.1)',
                                borderRadius: '6px'
                            }}
                        />
                        <Background variant="dots" gap={25} size={1} color="#e8e8e8" />
                    </ReactFlow>
                </div>

                <StatusPanel onTransitionClick={handleTransitionClick} />
            </div>

            <style>
                {`
                @keyframes pulse {
                    0% { opacity: 1; }
                    50% { opacity: 0.7; }
                    100% { opacity: 1; }
                }
                
                @keyframes fadeInOut {
                    0% { opacity: 0; transform: translateX(-50%) translateY(-10px); }
                    15% { opacity: 1; transform: translateX(-50%) translateY(0); }
                    85% { opacity: 1; transform: translateX(-50%) translateY(0); }
                    100% { opacity: 0; transform: translateX(-50%) translateY(-10px); }
                }
                
                button {
                    transition: all 0.2s ease;
                }
                
                button:hover:not(:disabled) {
                    transform: translateY(-2px);
                    box-shadow: 0 6px 12px rgba(0,0,0,0.2);
                }
                
                .react-flow__edge-path {
                    stroke-width: 1.5 !important;
                }
                
                .react-flow__edge.selected .react-flow__edge-path {
                    stroke-width: 2 !important;
                }
                
                .react-flow__node[data-type="transition"] {
                    border-radius: 6px !important;
                    font-size: 12px !important;
                    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                    transition: all 0.3s ease !important;
                }
                
                .react-flow__node[data-type="place"] {
                    border-radius: 50% !important;
                    min-width: 100px !important;
                    min-height: 100px !important;
                    display: flex !important;
                    align-items: center !important;
                    justify-content: center !important;
                }
                
                .react-flow__node:hover {
                    filter: brightness(1.05) !important;
                    z-index: 100 !important;
                }
                
                .react-flow {
                    width: 100% !important;
                    height: 100% !important;
                }
                
                .react-flow__renderer {
                    width: 100% !important;
                    height: 100% !important;
                }
                
                .react-flow__pane {
                    width: 100% !important;
                    height: 100% !important;
                }
                `}
            </style>
        </div>
    );
}

function App() {
    return (
        <ReactFlowProvider>
            <AppContent />
        </ReactFlowProvider>
    );
}

export default App;