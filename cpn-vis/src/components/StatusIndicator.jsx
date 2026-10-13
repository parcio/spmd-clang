import React from 'react';
import { useAtomValue } from 'jotai';
import {
    currentStateIdAtom,
    availableTransitionsAtom,
    fireAnimationAtom,
    elkAvailableAtom
} from '../atoms/petriNetAtoms';

const StatusIndicator = () => {
    const currentStateId = useAtomValue(currentStateIdAtom);
    const availableTransitions = useAtomValue(availableTransitionsAtom);
    const fireAnimation = useAtomValue(fireAnimationAtom);
    const elkAvailable = useAtomValue(elkAvailableAtom);

    return (
        <div style={{
            position: 'absolute',
            top: '15px',
            left: '15px',
            background: 'rgba(255,255,255,0.9)',
            padding: '8px 12px',
            borderRadius: '6px',
            zIndex: 10,
            fontSize: '12px',
            fontWeight: '600',
            border: '1px solid #e2e8f0',
            boxShadow: '0 1px 4px rgba(0,0,0,0.1)',
            color: '#4a5568',
            maxWidth: '250px',
            pointerEvents: 'none'
        }}>
            <div style={{ marginBottom: '4px' }}>
                <strong>Status:</strong> State {currentStateId}
            </div>
            <div style={{
                fontSize: '11px',
                color: '#718096',
                marginTop: '4px'
            }}>
                {fireAnimation ?
                    <span style={{ color: '#ff9800', fontWeight: 'bold' }}>
                        Processing...
                    </span> :
                    availableTransitions.length > 0 ?
                        <span>
                            {availableTransitions.length} transition{availableTransitions.length > 1 ? 's' : ''} available
                        </span> :
                        <span>No transitions available</span>
                }
            </div>
            {elkAvailable ? (
                <div style={{
                    fontSize: '10px',
                    color: '#4caf50',
                    marginTop: '4px',
                    fontWeight: 'bold'
                }}>
                    ✓ ELK.js Layout
                </div>
            ) : (
                <div style={{
                    fontSize: '10px',
                    color: '#ff9800',
                    marginTop: '4px',
                    fontWeight: 'bold'
                }}>
                    ⚠ Fallback Layout
                </div>
            )}
        </div>
    );
};

export default StatusIndicator;