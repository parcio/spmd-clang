import { useState } from "react";
import PropTypes from 'prop-types';
import useStore from '../../store';
import { useShallow } from "zustand/react/shallow";
import { useParams } from "react-router-dom";
import { findFunctionData, findGroupNodesRec } from "../edges/edgeHandler";


const selector = (state) => ({
    graphData : state.graphData,
    updateNodeState: state.updateNodeState,
    addBridgeEdge : state.addBridgeEdge,
    deleteBridgeEdge : state.deleteBridgeEdge
})


function ExtendFunctionButton({ functionNodeId }) {
    const { graphData, updateNodeState, addBridgeEdge, deleteBridgeEdge } = useStore(useShallow(selector));
    const { graphId } = useParams();
    const graph = graphData[graphId];
    const functionNode = graph?.nodes?.find(node => node.id === functionNodeId) || { hidden: false };

    const hidden = functionNode.hidden;
    const iconExtended = "□";
    const iconNotExtended = "_";

    function handleClick() {
        // detect if this function node is a duplicated (side-by-side) version with suffix
        const suffixMatch = functionNodeId.match(/(_process_\d+)$/);
        const suffix = suffixMatch ? suffixMatch[1] : null;
        const baseFunctionId = suffix ? functionNodeId.replace(suffix, '') : functionNodeId;

        let { bridgeEdgesGroup, groupNodes } = findFunctionData(baseFunctionId, graph.nodes, graph.groups, graph.edges);
        if (!bridgeEdgesGroup || !groupNodes) {
            console.debug("ExtendFunctionButton: no function data for", baseFunctionId, { graphId, functionNodeId });
            return;
        }

        if (hidden) {
            // Expand: show function and its children (use suffixed ids when applicable)
            console.debug("ExtendFunctionButton: expanding", { functionNodeId, baseFunctionId, suffix, groupNodesCount: groupNodes.length, bridgeEdgesCount: bridgeEdgesGroup.length });
            updateNodeState(graphId, functionNodeId, false);
            groupNodes.forEach((node) => {
                const targetId = suffix ? `${node.id}${suffix}` : node.id;
                updateNodeState(graphId, targetId, false);
            });
            bridgeEdgesGroup.forEach((edge) => {
                const targetEdge = suffix
                    ? { ...edge, id: `${edge.id}${suffix}`, source: `${edge.source}${suffix}`, target: `${edge.target}${suffix}`, label: functionNodeId }
                    : edge;
                deleteBridgeEdge(Number(graphId), targetEdge);
            });
        } else {
            // Collapse: hide function and its children (use suffixed ids when applicable)
            console.debug("ExtendFunctionButton: collapsing", { functionNodeId, baseFunctionId, suffix, groupNodesCount: groupNodes.length, bridgeEdgesCount: bridgeEdgesGroup.length });
            updateNodeState(graphId, functionNodeId, true);
            groupNodes.forEach((node) => {
                const targetId = suffix ? `${node.id}${suffix}` : node.id;
                updateNodeState(graphId, targetId, true);
            });
            bridgeEdgesGroup.forEach((edge) => {
                const targetEdge = suffix
                    ? { ...edge, id: `${edge.id}${suffix}`, source: `${edge.source}${suffix}`, target: `${edge.target}${suffix}`, label: functionNodeId }
                    : edge;
                addBridgeEdge(Number(graphId), targetEdge);
            });
        }
    }

    return (
        <button onClick={handleClick} className="extend-node-from-function">
            {hidden ? iconExtended : iconNotExtended}
        </button>
    );
}

ExtendFunctionButton.propTypes = {
    functionNodeId: PropTypes.string.isRequired,
    onToggle: PropTypes.func,
    collapsed: PropTypes.bool
};

export default ExtendFunctionButton;