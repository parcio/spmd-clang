import React from 'react';
import { useSetAtom } from 'jotai';
import { showClickHelpAtom } from '../atoms/petriNetAtoms';

const ClickHelpMessage = () => {
    const setShowClickHelp = useSetAtom(showClickHelpAtom);

    return (
        <div style={{
            position: 'absolute',
            top: '80px',
            left: '50%',
            transform: 'translateX(-50%)',
            background: 'linear-gradient(135deg, #2196f3, #1976d2)',
            color: 'white',
            padding: '12px 40px 12px 24px',
            borderRadius: '8px',
            zIndex: 20,
            fontWeight: 'bold',
            fontSize: '14px',
            boxShadow: '0 4px 12px rgba(33, 150, 243, 0.4)',
            animation: 'fadeInOut 3s ease-in-out',
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            pointerEvents: 'auto'
        }}>
            <span>⚡</span>
            <div>
                <div>Hold <strong>Ctrl</strong> and click blue transitions!</div>
                <div style={{ fontSize: '12px', opacity: 0.9, marginTop: '4px' }}>
                    Press Space to fire if only one is available
                </div>
            </div>
            <button
                onClick={() => setShowClickHelp(false)}
                style={{
                    position: 'absolute',
                    top: '8px',
                    right: '8px',
                    background: 'transparent',
                    border: 'none',
                    color: 'white',
                    fontSize: '16px',
                    cursor: 'pointer',
                    padding: '4px',
                    borderRadius: '4px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    width: '24px',
                    height: '24px',
                    transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.2)'}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
            >
                ×
            </button>
        </div>
    );
};

export default ClickHelpMessage;