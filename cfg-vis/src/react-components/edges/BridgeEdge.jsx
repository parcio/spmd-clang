import { useCallback } from 'react';
// eslint-disable-next-line no-unused-vars
import { BaseEdge, useStore, getSmoothStepPath, getSimpleBezierPath, getBezierPath, EdgeLabelRenderer, useEdges } from 'reactflow';
import { getEdgeParams } from './utils';

import ExtendFunctionButton from '../nodes/ExtendFunctionButton';

// eslint-disable-next-line react/prop-types
export default function BridgeEdge({ id, source, target, markerEnd, style, label, functionNodeId }) {

  // hook to suscribe to node states due to layout changes
  const sourceNode = useStore(useCallback((store) => store.nodeInternals.get(source), [source]));
  const targetNode = useStore(useCallback((store) => store.nodeInternals.get(target), [target]));

  if(!(sourceNode && targetNode)) {
    return null;
  }

  const { sx, sy, tx, ty, sourcePos, targetPos } = getEdgeParams(sourceNode, targetNode);

  // [ getBezierPath --> curved, getSimpleBezierPath --> medium-curved, getSmoothStepPath --> nearly straight ]
  const [edgePath, labelX, labelY] = getSimpleBezierPath({ 
    sourceX: sx,
    sourceY: sy,
    sourcePosition: sourcePos,
    targetPosition: targetPos,
    targetX: tx,
    targetY: ty
  })

  let thickerEdges = { ...style, strokeWidth: 2}

  return (
    <>
    <path
      id={id}
      className="react-flow__edge-path"
      d={edgePath}
      markerEnd={markerEnd}
      style={thickerEdges}
    />
    <EdgeLabelRenderer>
    <div
      style={{
        position: 'absolute',
        transform: `translate(-50%, -50%) translate(${labelX}px,${labelY}px)`,
        // everything inside EdgeLabelRenderer has no pointer events by default
        // if you have an interactive element, set pointer-events: all
        pointerEvents: 'all',
        zIndex: 2001
      }}
      >
      <span style={{ display: 'inline-flex', alignItems: 'center', gap: '0.5rem', whiteSpace: 'nowrap', background: '#6a839c', borderRadius: '3px', padding: '4px 8px', color: 'black', fontFamily: "'Courier New', monospace", opacity: 0.8 }}>
        <h1 style={{ display: 'inline', margin: 0, fontSize: '11px' }}>{label}</h1>
        {label && <ExtendFunctionButton functionNodeId={label} />}
      </span>
    </div>
    </EdgeLabelRenderer>
    </>
  );
}