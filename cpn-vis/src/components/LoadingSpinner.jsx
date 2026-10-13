import React from 'react';

const LoadingSpinner = () => {
    return (
        <div style={{
            width: '100vw',
            height: '100vh',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            background: '#f7fafc'
        }}>
            <div style={{ textAlign: 'center' }}>
                <h3>Loading Petri Net with ELK.js Layout...</h3>
                <div style={{
                    width: '40px',
                    height: '40px',
                    border: '4px solid #e2e8f0',
                    borderTop: '4px solid #2196f3',
                    borderRadius: '50%',
                    margin: '20px auto',
                    animation: 'spin 1s linear infinite'
                }} />
                <p style={{ color: '#666', fontSize: '14px' }}>
                    Applying professional graph layout...
                </p>
            </div>
            <style>{`
                @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
            `}</style>
        </div>
    );
};

export default LoadingSpinner;