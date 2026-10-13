// returns the parameters (sx, sy, tx, ty, sourcePos, targetPos) you need to create an edge
import { Position, internalsSymbol } from 'reactflow';
import useStore from '../../store';
import { useShallow } from 'zustand/react/shallow';

const selector = (state) => ({
  nodes: state.nodes,
  groups: state.groups
});

// this helper function returns the intersection point
// of the line between the center of the intersectionNode and the target node
function getNodeIntersection(intersectionNode, targetNode) {
  // https://math.stackexchange.com/questions/1724792/an-algorithm-for-finding-the-intersection-point-between-a-center-of-vision-and-a
  const {
    width: intersectionNodeWidth,
    height: intersectionNodeHeight,
    positionAbsolute: intersectionNodePosition,
  } = intersectionNode;
  const targetPosition = targetNode.positionAbsolute;

  const w = intersectionNodeWidth / 2;
  const h = intersectionNodeHeight / 2;

  const x2 = intersectionNodePosition.x + w;
  const y2 = intersectionNodePosition.y + h;
  const x1 = targetPosition.x + targetNode.width / 2;
  const y1 = targetPosition.y + targetNode.height / 2;

  const xx1 = (x1 - x2) / (2 * w) - (y1 - y2) / (2 * h);
  const yy1 = (x1 - x2) / (2 * w) + (y1 - y2) / (2 * h);
  const a = 1 / (Math.abs(xx1) + Math.abs(yy1));
  const xx3 = a * xx1;
  const yy3 = a * yy1;
  const x = w * (xx3 + yy3) + x2;
  const y = h * (-xx3 + yy3) + y2;

  return { x, y };
}

// returns the position (top,right,bottom or right) passed node compared to the intersection point
function getEdgePosition(node, intersectionPoint) {
  const n = { ...node.positionAbsolute, ...node };
  const nx = Math.round(n.x);
  const ny = Math.round(n.y);
  const px = Math.round(intersectionPoint.x);
  const py = Math.round(intersectionPoint.y);

  if (px <= nx + 1) {
    return Position.Left;
  }
  if (px >= nx + n.width - 1) {
    return Position.Right;
  }
  if (py <= ny + 1) {
    return Position.Top;
  }
  if (py >= n.y + n.height - 1) {
    return Position.Bottom;
  }

  return Position.Top;
}

// returns the parameters (sx, sy, tx, ty, sourcePos, targetPos) you need to create an edge
export function getEdgeParams(source, target) {

  const { nodes } = useStore(useShallow(selector));

  // default Floating Edge Connections between cfg-nodes
  let sourceConnectionPoint = getNodeIntersection(source, target);
  let targetConnectionPoint = getNodeIntersection(target, source);

  let sourcePos = getEdgePosition(source, sourceConnectionPoint);
  let targetPos = getEdgePosition(target, targetConnectionPoint);

  let sourcePosAbsolute = source.positionAbsolute;
  let targetPosAbsolute = target.positionAbsolute;

  // check wether edge is going to/coming from a group node or the first/last inside the group node 
  // --> fix one end or both ends of edge to group handle to assure correct group layouting with ELK

  // extra Funktion hier?

  if(source.type === 'function-node' && !(target.type === 'function-node')) {
    
    let childrenFunction = nodes.find((x) => x.id === source.id).children;
    let targetIsFunctionChild = (childrenFunction.find((x) => x.id === target.id)) !== undefined ? true : false;

    if(targetIsFunctionChild) {   // edge is the first one inside Function Subflow --> connect to 'function-start' handle of group node
      let handleFunctionStart = source[internalsSymbol].handleBounds.target.find((handle) => handle.id === 'function-start');
      sourceConnectionPoint = { x: handleFunctionStart.x + sourcePosAbsolute.x + 4, y: handleFunctionStart.y + sourcePosAbsolute.y + 8 };
      sourcePos = Position.Bottom; // damit nach unten weggehend
    }
    else {  // edge is first one after Function Subflow --> connect to 'function-end' handle
      let handleFunctionEnd = source[internalsSymbol].handleBounds.source.find((handle) => handle.id === 'function-end');
      sourceConnectionPoint = { x: handleFunctionEnd.x + sourcePosAbsolute.x + 4, y: handleFunctionEnd.y + sourcePosAbsolute.y + 8 };
      sourcePos = Position.Bottom;
    }
  }
  else if(!(source.type === 'function-node') && target.type === 'function-node') {

    let childrenFunction = nodes.find((x) => x.id === target.id).children;
    let sourceIsFunctionChild = (childrenFunction.find((x) => x.id === source.id)) !== undefined ? true : false;

    if(sourceIsFunctionChild) {   // edge is the last one inside the function --> connect to 'function-end' handle
      let handleFunctionEnd = target[internalsSymbol].handleBounds.source.find((handle) => handle.id === 'function-end');
      targetConnectionPoint = { x: handleFunctionEnd.x + targetPosAbsolute.x + 4, y: handleFunctionEnd.y + targetPosAbsolute.y };
      targetPos = Position.Top;
    }
    else {  // edge is leading into function-group / last one before function group --> connect to 'function-start' handle
      let handleFunctionStart = target[internalsSymbol].handleBounds.target.find((handle) => handle.id === 'function-start');
      targetConnectionPoint =  { x: handleFunctionStart.x + targetPosAbsolute.x + 4, y: handleFunctionStart.y + targetPosAbsolute.y };
      targetPos = Position.Top;
    }
  }
  else if(source.type === 'function-node' && target.type === 'function-node') {

    let handleSource = source[internalsSymbol].handleBounds.source.find((handle) => handle.id === 'function-end');
    let handleTarget = target[internalsSymbol].handleBounds.target.find((handle) => handle.id === 'function-start');
    
    sourceConnectionPoint = { x: handleSource.x + sourcePosAbsolute.x + 4, y: handleSource.y + sourcePosAbsolute.y };
    targetConnectionPoint = { x: handleTarget.x + targetPosAbsolute.x + 4, y: handleTarget.y + targetPosAbsolute.y };
    sourcePos = Position.Bottom;
    targetPos = Position.Top;
  }
  else {
    //console.log('Normal cfg-node Connection kept.');
  }

  return {
    sx: sourceConnectionPoint.x,
    sy: sourceConnectionPoint.y,
    tx: targetConnectionPoint.x,
    ty: targetConnectionPoint.y,
    sourcePos,
    targetPos,
  };
}