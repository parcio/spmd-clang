import React from 'react';
import { useAtomValue } from 'jotai';
import {
    elkAvailableAtom,
    currentStateIdAtom,
    isAtBranchingPointAtom,
    isAtFinalStateAtom,
    fireAnimationAtom,
    firingTransitionAtom,
    lastClickedTransitionAtom,
    availableTransitionsAtom
} from '../atoms/petriNetAtoms';

const ControlPanel = ({ onNext, onPrevious, onReset, onReapplyLayout }) => {
    const elkAvailable = useAtomValue(elkAvailableAtom);
    const currentStateId = useAtomValue(currentStateIdAtom);
    const isAtBranchingPoint = useAtomValue(isAtBranchingPointAtom);
    const isAtFinalState = useAtomValue(isAtFinalStateAtom);
    const fireAnimation = useAtomValue(fireAnimationAtom);
    const firingTransition = useAtomValue(firingTransitionAtom);
    const lastClickedTransition = useAtomValue(lastClickedTransitionAtom);
    const availableTransitions = useAtomValue(availableTransitionsAtom);

    const stateColor = isAtBranchingPoint ? '#9c27b0' : isAtFinalState ? '#4caf50' : '#2196f3';

    return (
        <div style={{
            background: 'white',
            padding: '12px 20px',
            borderBottom: '1px solid #e2e8f0',
            display: 'flex',
            gap: '15px',
            alignItems: 'center',
            justifyContent: 'space-between',
            boxShadow: '0 1px 3px rgba(0,0,0,0.1)',
            flexShrink: 0,
            height: '70px',
            boxSizing: 'border-box'
        }}>
            <div style={{ display: 'flex', gap: '15px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: '#2d3748' }}>Petri Net Simulator</h3>

                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    <button
                        onClick={onReapplyLayout}
                        style={{
                            background: '#2196f3',
                            color: 'white',
                            border: 'none',
                            padding: '8px 16px',
                            borderRadius: '6px',
                            cursor: 'pointer',
                            fontWeight: '500',
                            fontSize: '14px',
                            transition: 'all 0.2s ease',
                            boxShadow: '0 2px 4px rgba(33, 150, 243, 0.3)'
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.transform = 'translateY(-2px)'}
                        onMouseLeave={(e) => e.currentTarget.style.transform = 'translateY(0)'}
                    >
                        {elkAvailable ? ' Reapply ELK Layout' : 'Reapply Layout'}
                    </button>
                </div>
            </div>

            <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                <div style={{
                    background: stateColor,
                    padding: '8px 16px',
                    borderRadius: '6px',
                    fontWeight: '600',
                    color: 'white',
                    fontSize: '14px',
                }}>
                    <span>State: {currentStateId}</span>
                    {fireAnimation && (
                        <span style={{
                            marginLeft: '10px',
                            background: '#ff9800',
                            padding: '4px 8px',
                            borderRadius: '4px',
                            fontSize: '12px',
                            fontWeight: 'bold',
                            animation: 'pulse 1s infinite'
                        }}>
                            {firingTransition}
                        </span>
                    )}
                    {lastClickedTransition && !fireAnimation && (
                        <span style={{
                            marginLeft: '10px',
                            background: '#1976d2',
                            padding: '4px 8px',
                            borderRadius: '4px',
                            fontSize: '12px',
                            fontWeight: 'bold'
                        }}>
                            ↪ {lastClickedTransition}
                        </span>
                    )}
                </div>

                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    <button
                        onClick={onPrevious}
                        disabled={fireAnimation}
                        style={{
                            background: '#757575',
                            color: 'white',
                            border: 'none',
                            padding: '10px 20px',
                            borderRadius: '6px',
                            cursor: fireAnimation ? 'not-allowed' : 'pointer',
                            fontWeight: '500',
                            fontSize: '14px',
                            minWidth: '100px',
                            opacity: fireAnimation ? 0.6 : 1,
                            transition: 'all 0.2s ease'
                        }}
                        onMouseEnter={(e) => !fireAnimation && (e.currentTarget.style.transform = 'translateY(-2px)')}
                        onMouseLeave={(e) => e.currentTarget.style.transform = 'translateY(0)'}
                    >
                        ← Back
                    </button>

                    <button
                        onClick={onNext}
                        disabled={availableTransitions.length === 0 || fireAnimation}
                        style={{
                            background: fireAnimation ? '#ff9800' :
                                availableTransitions.length === 0 ? '#e2e8f0' :
                                    isAtBranchingPoint ? '#9c27b0' : '#4caf50',
                            color: fireAnimation || availableTransitions.length === 0 ? '#a0aec0' : 'white',
                            border: 'none',
                            padding: '10px 20px',
                            borderRadius: '6px',
                            cursor: availableTransitions.length === 0 || fireAnimation ? 'not-allowed' : 'pointer',
                            fontWeight: '500',
                            fontSize: '14px',
                            minWidth: '140px',
                            transition: 'all 0.2s ease'
                        }}
                        onMouseEnter={(e) => !fireAnimation && availableTransitions.length > 0 && (e.currentTarget.style.transform = 'translateY(-2px)')}
                        onMouseLeave={(e) => e.currentTarget.style.transform = 'translateY(0)'}
                    >
                        {fireAnimation ? 'Processing...' :
                            availableTransitions.length > 1 ? 'Choose Transition ↓' :
                                isAtBranchingPoint ? 'Choose Path →' : 'Next →'}
                    </button>

                    <button
                        onClick={onReset}
                        disabled={fireAnimation}
                        style={{
                            background: '#f44336',
                            color: 'white',
                            border: 'none',
                            padding: '10px 20px',
                            borderRadius: '6px',
                            cursor: fireAnimation ? 'not-allowed' : 'pointer',
                            fontWeight: '500',
                            fontSize: '14px',
                            transition: 'all 0.2s ease'
                        }}
                        onMouseEnter={(e) => !fireAnimation && (e.currentTarget.style.transform = 'translateY(-2px)')}
                        onMouseLeave={(e) => e.currentTarget.style.transform = 'translateY(0)'}
                    >
                        Reset
                    </button>
                </div>
            </div>
        </div>
    );
};

export default ControlPanel;