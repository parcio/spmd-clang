import { useCallback } from 'react';
// eslint-disable-next-line no-unused-vars
import { BaseEdge, useStore, getStraightPath, getSmoothStepPath, getSimpleBezierPath, getBezierPath, EdgeLabelRenderer, useEdges, MarkerType } from 'reactflow';
import { getEdgeParams } from './utils';
import { color } from 'd3';

// eslint-disable-next-line react/prop-types
export default function LabledDirectedEdge({ id, source, target, markerEnd, style, label }) {

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
        fontSize: 5,
        // everything inside EdgeLabelRenderer has no pointer events by default
        // if you have an interactive element, set pointer-events: all
        pointerEvents: 'all',
        fontFamily: 'Courier New',
        background: 'ECF2F8',
        zIndex: 2001
      }}
      >
      <h1> {label} </h1>
    </div>
    </EdgeLabelRenderer>
    </>
  );
}
