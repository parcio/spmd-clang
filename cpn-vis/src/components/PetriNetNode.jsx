import React, { useMemo } from 'react';
import { Handle, Position } from 'reactflow';
import {parseHtmlLabel} from "../utils/htmlParser.js";

const PetriNetNode = ({ data, selected }) => {
    const {
        type,
        label,
        htmlLabel,
        tokens = [],
        firing = false
    } = data;

    const isPlace = type === 'place';
    const isTransition = type === 'transition';
    const displayLabel = htmlLabel || label;

    const parsedHtmlLabel = useMemo(() => {
        return parseHtmlLabel(displayLabel);
    }, [displayLabel]);

    const { nodeWidth, nodeHeight, fontSize, padding } = useMemo(() => {
        const basePlaceSize = 80;
        const baseTransitionWidth = 100;
        const baseTransitionHeight = 50;

        const getTextWidth = (text) => {
            const cleanText = text?.replace(/<[^>]*>/g, '') || '';
            const charCount = cleanText.length || 0;
            return Math.max(100, Math.min(220, charCount * 7 + 30));
        };

        const getTextHeight = (text, tokens) => {
            let lines = 1;

            const cleanText = text?.replace(/<[^>]*>/g, '') || '';
            if (cleanText && cleanText.includes('\n')) {
                lines += cleanText.split('\n').length - 1;
            }
            if (text && text.includes('<BR')) {
                const brCount = (text.match(/<BR\/?>/gi) || []).length;
                lines += brCount;
            }
            if (tokens && tokens.length > 0 && tokens[0]) {
                lines += 1;
            }

            const charCount = cleanText.length;
            if (charCount > 60) {
                lines += Math.floor((charCount - 60) / 40);
            }

            return Math.max(50, lines * 26);
        };

        if (isPlace) {
            const width = basePlaceSize;
            const height = basePlaceSize;
            const cleanText = displayLabel?.replace(/<[^>]*>/g, '') || '';
            const fontSize = Math.min(14, Math.max(10, 14 - (cleanText.length - 8) * 0.5));

            return {
                nodeWidth: width,
                nodeHeight: height,
                fontSize,
                padding: 10
            };
        }

        if (isTransition) {
            const width = getTextWidth(displayLabel);
            const height = getTextHeight(displayLabel, tokens);
            const cleanText = displayLabel?.replace(/<[^>]*>/g, '') || '';
            const fontSize = Math.min(13, Math.max(10, 13 - (cleanText.length - 10) * 0.2));

            return {
                nodeWidth: width,
                nodeHeight: height,
                fontSize,
                padding: 12
            };
        }

        return {
            nodeWidth: baseTransitionWidth,
            nodeHeight: baseTransitionHeight,
            fontSize: 14,
            padding: 10
        };
    }, [displayLabel, tokens, isPlace, isTransition]);

    const getColors = () => {
        if (isPlace) {
            const tokenCount = tokens.length;
            return {
                border: tokenCount > 0 ? '#2b6cb0' : '#90cdf4',
                bg: tokenCount > 0 ? '#ebf8ff' : '#f7fafc',
                token: '#2b6cb0',
                text: '#2d3748',
            };
        }

        if (isTransition) {
            if (firing) {
                return {
                    border: '#f56565',
                    bg: '#fff5f5',
                    text: '#c53030',
                    accent: '#fc8181',
                };
            }
            return {
                border: '#38a169',
                bg: '#f0fff4',
                text: '#22543d',
                accent: '#68d391',
            };
        }

        return {
            border: '#a0aec0',
            bg: '#f7fafc',
            text: '#4a5568',
        };
    };

    const colors = getColors();
    const tokenCount = tokens.length;

    const nodeStyle = {
        borderRadius: isPlace ? '50%' : '8px',
        width: nodeWidth,
        height: nodeHeight,
        minWidth: isPlace ? 80 : 100,
        minHeight: isPlace ? 80 : 50,
        border: `3px solid ${colors.border}`,
        background: colors.bg,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        position: 'relative',
        fontSize: fontSize,
        fontWeight: 'normal',
        color: colors.text,
        boxShadow: selected ? `0 0 0 3px ${colors.border}40` : '0 2px 4px rgba(0,0,0,0.1)',
        transition: 'all 0.3s ease',
        overflow: 'hidden',
        padding: padding,
        wordBreak: 'break-word',
        textAlign: 'center',
        lineHeight: '1.3',
    };

    const labelLines = useMemo(() => {
        if (!displayLabel) return [''];
        const cleanText = displayLabel.replace(/<[^>]*>/g, '');
        return cleanText.split('\n').map(line => line.trim());
    }, [displayLabel]);

    const firstToken = tokens && tokens.length > 0 ? tokens[0] : null;

    const renderFiringEffect = () => {
        if (!isTransition || !firing) return null;

        return (
            <>
                <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    background: `radial-gradient(circle at center, 
                        rgba(245, 101, 101, 0.3) 0%, 
                        rgba(245, 101, 101, 0.1) 50%, 
                        transparent 100%)`,
                    animation: 'pulse 1.5s infinite',
                    pointerEvents: 'none',
                }} />

                <div style={{
                    position: 'absolute',
                    top: -6,
                    right: -6,
                    background: '#f56565',
                    color: 'white',
                    borderRadius: '50%',
                    width: '22px',
                    height: '22px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    border: '2px solid white',
                    boxShadow: '0 2px 8px rgba(245, 101, 101, 0.5)',
                    animation: 'pulse 1s infinite',
                    zIndex: 10,
                }}>
                </div>
            </>
        );
    };

    const renderTokenDots = () => {
        if (!isPlace || tokenCount === 0) return null;

        if (tokenCount === 1) {
            return (
                <div style={{
                    position: 'absolute',
                    top: '50%',
                    left: '50%',
                    transform: 'translate(-50%, -50%)',
                    width: '20px',
                    height: '20px',
                    borderRadius: '50%',
                    background: colors.token,
                    border: '2px solid white',
                    boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                }} />
            );
        }

        if (tokenCount === 2) {
            return (
                <>
                    <div style={{
                        position: 'absolute',
                        top: '30%',
                        left: '30%',
                        transform: 'translate(-50%, -50%)',
                        width: '14px',
                        height: '14px',
                        borderRadius: '50%',
                        background: colors.token,
                        border: '2px solid white',
                        boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                    }} />
                    <div style={{
                        position: 'absolute',
                        bottom: '30%',
                        right: '30%',
                        transform: 'translate(50%, 50%)',
                        width: '14px',
                        height: '14px',
                        borderRadius: '50%',
                        background: colors.token,
                        border: '2px solid white',
                        boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                    }} />
                </>
            );
        }

        if (tokenCount === 3) {
            return (
                <>
                    <div style={{
                        position: 'absolute',
                        top: '25%',
                        left: '50%',
                        transform: 'translate(-50%, -50%)',
                        width: '12px',
                        height: '12px',
                        borderRadius: '50%',
                        background: colors.token,
                        border: '2px solid white',
                        boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                    }} />
                    <div style={{
                        position: 'absolute',
                        bottom: '25%',
                        left: '30%',
                        transform: 'translate(-50%, 50%)',
                        width: '12px',
                        height: '12px',
                        borderRadius: '50%',
                        background: colors.token,
                        border: '2px solid white',
                        boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                    }} />
                    <div style={{
                        position: 'absolute',
                        bottom: '25%',
                        right: '30%',
                        transform: 'translate(50%, 50%)',
                        width: '12px',
                        height: '12px',
                        borderRadius: '50%',
                        background: colors.token,
                        border: '2px solid white',
                        boxShadow: '0 1px 3px rgba(0,0,0,0.2)',
                    }} />
                </>
            );
        }

        return (
            <div style={{
                position: 'absolute',
                bottom: '12%',
                left: '50%',
                transform: 'translateX(-50%)',
                background: colors.token,
                color: 'white',
                fontSize: '10px',
                fontWeight: 'bold',
                borderRadius: '12px',
                minWidth: '24px',
                height: '20px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '0 6px',
                border: '1.5px solid white',
                boxShadow: '0 1px 4px rgba(0,0,0,0.2)',
                zIndex: 10,
                whiteSpace: 'nowrap',
            }}>
                {tokenCount}
            </div>
        )
    };

    const renderStatusIndicator = () => {
        if (!isPlace || tokenCount > 0) return null;

        return (
            <div style={{
                position: 'absolute',
                bottom: -18,
                left: '50%',
                transform: 'translateX(-50%)',
                fontSize: '9px',
                color: '#a0aec0',
                whiteSpace: 'nowrap',
                background: 'rgba(255,255,255,0.9)',
                padding: '1px 6px',
                borderRadius: '3px',
                border: '1px solid #e2e8f0',
            }}>
                empty
            </div>
        );
    };

    const renderHtmlContent = () => {
        if (!parsedHtmlLabel || !parsedHtmlLabel.includes('<')) {
            return (
                <div style={{
                    zIndex: 2,
                    position: 'relative',
                    width: '100%',
                }}>
                    {labelLines.map((line, index) => (
                        <div
                            key={index}
                            style={{
                                marginBottom: index < labelLines.length - 1 ? '2px' : '0',
                            }}
                        >
                            {line}
                        </div>
                    ))}
                </div>
            );
        }

        return (
            <div
                style={{
                    zIndex: 2,
                    position: 'relative',
                    width: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                }}
                dangerouslySetInnerHTML={{ __html: parsedHtmlLabel }}
            />
        );
    };

    return (
        <div style={nodeStyle}>
            {renderHtmlContent()}

            {isPlace && firstToken && (
                <div style={{
                    fontSize: Math.max(9, fontSize - 3),
                    color: '#4a5568',
                    marginTop: '4px',
                    opacity: 0.8,
                    fontWeight: 'normal',
                    maxWidth: '100%',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                    padding: '0 5px',
                    position: 'relative',
                    zIndex: 2,
                }}>
                    {firstToken}
                </div>
            )}

            {isTransition && tokenCount > 0 && (
                <div style={{
                    fontSize: Math.max(10, fontSize - 2),
                    color: colors.text,
                    marginTop: '4px',
                    opacity: 0.7,
                    fontWeight: 'normal',
                    fontStyle: 'italic',
                    position: 'relative',
                    zIndex: 2,
                }}>
                    {tokenCount} token{tokenCount !== 1 ? 's' : ''}
                </div>
            )}

            {renderTokenDots()}
            {renderFiringEffect()}
            {renderStatusIndicator()}

            {isPlace ? (
                <>
                    <Handle
                        type="target"
                        position={Position.Left}
                        style={{
                            background: colors.border,
                            width: 10,
                            height: 10,
                            border: '2px solid white',
                            top: '50%',
                            transform: 'translateY(-50%)',
                        }}
                    />
                    <Handle
                        type="source"
                        position={Position.Right}
                        style={{
                            background: colors.border,
                            width: 10,
                            height: 10,
                            border: '2px solid white',
                            top: '50%',
                            transform: 'translateY(-50%)',
                        }}
                    />
                </>
            ) : (
                <>
                    <Handle
                        type="target"
                        position={Position.Left}
                        style={{
                            background: colors.border,
                            width: 10,
                            height: 10,
                            border: '2px solid white',
                            top: '50%',
                            transform: 'translateY(-50%)',
                        }}
                    />
                    <Handle
                        type="source"
                        position={Position.Right}
                        style={{
                            background: colors.border,
                            width: 10,
                            height: 10,
                            border: '2px solid white',
                            top: '50%',
                            transform: 'translateY(-50%)',
                        }}
                    />
                </>
            )}
        </div>
    );
};

export default PetriNetNode;