import React from 'react';
import { useAtomValue, useSetAtom } from 'jotai';
import {
    currentStateAtom,
    currentStateIdAtom,
    isAtBranchingPointAtom,
    availableTransitionsAtom,
    fireAnimationAtom,
    firingTransitionAtom,
    selectedPathsAtom,
    transitionMapAtom
} from '../atoms/petriNetAtoms';
import { showClickHelpAtom } from '../atoms/petriNetAtoms';

const StatusPanel = ({ onTransitionClick }) => {
    const currentState = useAtomValue(currentStateAtom);
    const currentStateId = useAtomValue(currentStateIdAtom);
    const isAtBranchingPoint = useAtomValue(isAtBranchingPointAtom);
    const availableTransitions = useAtomValue(availableTransitionsAtom);
    const fireAnimation = useAtomValue(fireAnimationAtom);
    const firingTransition = useAtomValue(firingTransitionAtom);
    const selectedPaths = useAtomValue(selectedPathsAtom);
    const transitionMap = useAtomValue(transitionMapAtom);
    const setShowClickHelp = useSetAtom(showClickHelpAtom);

    const getGridColumns = (count) => {
        if (count === 1) return '1fr';
        if (count === 2) return 'repeat(2, 1fr)';
        if (count <= 6) return 'repeat(3, 1fr)';
        if (count <= 12) return 'repeat(4, 1fr)';
        return 'repeat(5, 1fr)';
    };

    return (
        <div style={{
            background: 'white',
            padding: '15px 20px',
            borderTop: '1px solid #e2e8f0',
            maxHeight: '200px',
            overflow: 'auto',
            fontSize: '13px',
            boxShadow: '0 -1px 3px rgba(0,0,0,0.1)',
            flexShrink: 0,
            height: 'auto',
            minHeight: '120px'
        }}>
            <h4 style={{ margin: '0 0 12px 0', color: '#2d3748', fontSize: '14px' }}>
                State {currentStateId} • {fireAnimation ? ` Processing ${firingTransition}` :
                isAtBranchingPoint ? ' Multiple Paths' : 'Current Status'}
            </h4>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '25px' }}>
                <div>
                    <div style={{ fontWeight: '600', color: '#4a5568', marginBottom: '8px' }}>
                        Current Tokens
                    </div>
                    <div style={{
                        maxHeight: '120px',
                        overflowY: 'auto',
                        paddingRight: '5px'
                    }}>
                        {currentState && Object.entries(currentState.marking)
                            .filter(([_, tokens]) => tokens.length > 0)
                            .map(([place, tokens]) => (
                                <div key={place} style={{
                                    margin: '4px 0',
                                    padding: '6px 8px',
                                    background: '#f0fff4',
                                    borderLeft: '4px solid #4caf50',
                                    borderRadius: '4px',
                                }}>
                                    <span style={{ fontWeight: '600', color: '#2d3748' }}>{place}:</span>
                                    <div style={{
                                        marginLeft: '8px',
                                        color: '#2e7d32',
                                        fontFamily: 'monospace',
                                        fontSize: '11px',
                                        wordBreak: 'break-word'
                                    }}>
                                        {tokens.join(', ')}
                                    </div>
                                </div>
                            ))
                        }
                        {(!currentState || Object.entries(currentState.marking).filter(([_, tokens]) => tokens.length > 0).length === 0) && (
                            <div style={{ color: '#a0aec0', textAlign: 'center', padding: '20px' }}>
                                No tokens in any place
                            </div>
                        )}
                    </div>
                </div>

                <div>
                    <div style={{ fontWeight: '600', color: '#4a5568', marginBottom: '8px' }}>
                        {fireAnimation ? ' Processing Transition' :
                            isAtBranchingPoint ? ' Available Paths' : 'Available Transitions'}
                    </div>

                    <div style={{
                        maxHeight: '140px',
                        overflowY: 'auto',
                        paddingRight: '5px'
                    }}>
                        {fireAnimation ? (
                            <div style={{
                                background: 'linear-gradient(135deg, #fff3e0, #ffecb3)',
                                padding: '15px',
                                borderRadius: '8px',
                                border: '2px solid #ff9800',
                                animation: 'pulse 2s infinite'
                            }}>
                                <div style={{
                                    fontWeight: 'bold',
                                    color: '#ef6c00',
                                    marginBottom: '10px',
                                    fontSize: '15px',
                                    display: 'flex',
                                    alignItems: 'center',
                                    gap: '8px'
                                }}>
                                    <span>Processing {firingTransition}</span>
                                </div>
                                <p style={{ color: '#666', fontSize: '13px' }}>
                                    Moving to State {transitionMap[currentStateId]?.[firingTransition]}
                                </p>
                            </div>
                        ) : isAtBranchingPoint ? (
                            <div style={{
                                background: '#f3e5f5',
                                padding: '15px',
                                borderRadius: '8px',
                                border: '2px solid #9c27b0'
                            }}>
                                <div style={{
                                    fontWeight: 'bold',
                                    color: '#7b1fa2',
                                    marginBottom: '10px',
                                    fontSize: '15px',
                                    display: 'flex',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    flexWrap: 'wrap',
                                    gap: '8px'
                                }}>
                                    <span>{availableTransitions.length} Paths Available</span>
                                    <span style={{
                                        background: '#9c27b0',
                                        color: 'white',
                                        padding: '2px 8px',
                                        borderRadius: '4px',
                                        fontSize: '12px',
                                        whiteSpace: 'nowrap'
                                    }}>
                                        {availableTransitions.length} choices
                                    </span>
                                </div>
                                <div style={{
                                    display: 'grid',
                                    gridTemplateColumns: getGridColumns(availableTransitions.length),
                                    gap: '10px',
                                    marginTop: '10px'
                                }}>
                                    {availableTransitions.map((transition) => (
                                        <button
                                            key={transition.transitionId}
                                            onClick={() => onTransitionClick(transition.transitionId)}
                                            style={{
                                                padding: '12px 8px',
                                                background: 'linear-gradient(135deg, #bbdefb, #90caf9)',
                                                borderRadius: '6px',
                                                border: '3px solid #2196f3',
                                                cursor: 'pointer',
                                                transition: 'all 0.2s',
                                                textAlign: 'center',
                                                boxShadow: '0 4px 8px rgba(33, 150, 243, 0.3)',
                                                overflow: 'hidden',
                                                textOverflow: 'ellipsis',
                                                wordBreak: 'break-word'
                                            }}
                                            title={transition.transitionId}
                                        >
                                            <div style={{
                                                fontWeight: 'bold',
                                                color: '#0d47a1',
                                                fontSize: '13px'
                                            }}>
                                                {transition.transitionId.length > 20
                                                    ? transition.transitionId.substring(0, 18) + '...'
                                                    : transition.transitionId}
                                            </div>
                                            <div style={{
                                                fontSize: '11px',
                                                color: '#1565c0',
                                                marginTop: '4px'
                                            }}>
                                                → State {transition.targetStateId}
                                            </div>
                                            <div style={{
                                                fontSize: '9px',
                                                color: '#1976d2',
                                                marginTop: '6px',
                                                fontStyle: 'italic'
                                            }}>
                                                Ctrl+Click
                                            </div>
                                        </button>
                                    ))}
                                </div>
                            </div>
                        ) : (
                            <div style={{
                                padding: '12px',
                                background: '#f7fafc',
                                borderRadius: '8px'
                            }}>
                                {availableTransitions.length > 0 ? (
                                    <div style={{
                                        display: 'flex',
                                        flexWrap: 'wrap',
                                        gap: '8px',
                                        marginBottom: '12px'
                                    }}>
                                        {availableTransitions.map(t => (
                                            <span key={t.transitionId} style={{
                                                background: '#e3f2fd',
                                                color: '#1976d2',
                                                padding: '4px 10px',
                                                borderRadius: '16px',
                                                fontSize: '12px',
                                                fontWeight: 'bold',
                                                fontFamily: 'monospace'
                                            }}>
                                                {t.transitionId}
                                            </span>
                                        ))}
                                    </div>
                                ) : (
                                    <div style={{ color: '#a0aec0', textAlign: 'center', padding: '20px' }}>
                                        No transitions available
                                    </div>
                                )}
                                <div style={{ marginTop: '12px' }}>
                                    <strong>How to Fire:</strong>
                                    <div style={{
                                        marginTop: '4px',
                                        padding: '6px',
                                        background: '#e3f2fd',
                                        borderRadius: '4px',
                                        fontSize: '12px'
                                    }}>
                                        <div>• Hold <strong>Ctrl</strong> and click blue transitions</div>
                                        <div>• Press <strong>Space</strong> if only one option is available</div>
                                        <div>• Press <strong>Next</strong> if only one option is available</div>
                                    </div>
                                </div>
                            </div>
                        )}
                    </div>
                </div>

                <div>
                    <div style={{ fontWeight: '600', color: '#4a5568', marginBottom: '8px' }}>
                        Simulation Info
                    </div>
                    <div style={{
                        background: '#f7fafc',
                        padding: '12px',
                        borderRadius: '8px'
                    }}>
                        <div style={{ marginBottom: '6px', display: 'flex', justifyContent: 'space-between' }}>
                            <span>Current State:</span>
                            <span style={{ fontWeight: '600' }}>{currentStateId}</span>
                        </div>
                        <div style={{ marginBottom: '6px', display: 'flex', justifyContent: 'space-between' }}>
                            <span>Available Choices:</span>
                            <span style={{
                                fontWeight: 'bold',
                                color: availableTransitions.length > 0 ? '#1976d2' : '#757575'
                            }}>
                                {availableTransitions.length}
                            </span>
                        </div>
                        <div style={{
                            marginTop: '12px',
                            paddingTop: '12px',
                            borderTop: '1px solid #e2e8f0',
                            fontSize: '12px',
                            color: '#718096'
                        }}>
                            {fireAnimation
                                ? <span style={{ color: '#ff9800', fontWeight: 'bold' }}> Processing {firingTransition}...</span>
                                : availableTransitions.length > 0
                                    ? <span>Use <strong>Ctrl+Click</strong> on blue transitions</span>
                                    : <span>No transitions available from this state</span>
                            }
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default StatusPanel;