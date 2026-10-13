import useStore from '../../store';
import ExtendFunctionButton from './ExtendFunctionButton';
import { useState } from 'react';

function FunctionNode({ id, data }) {
    const graphId = Number(window.location.pathname.replace("/graphs/", ""));
    const nodes = useStore(state => state.graphData[graphId]?.nodes ?? []);
    const nd = nodes.find((node) => node.id === id);


    if (nd !== undefined) {
        const w = nd.width !== undefined ? nd.width : 400;
        const h = nd.height !== undefined ? nd.height : 250;

        // Ausgeweiteter Funktionsknoten
        return (
            <div className="function-node" style={{ width: w, height: h }}>
                <div style={{ display: 'flex', justifyContent: 'flex-end', alignItems: 'center', position: 'relative', width: '100%' }}>
                    <b>
                        <label htmlFor="name" style={{ color: 'black', fontSize: 11,marginLeft: '6px', marginRight: 'auto' }}>
                            {data.label}
                        </label>
                    </b>
                    <div style={{ marginLeft: 'auto', marginRight: '6px' }}>
                        <ExtendFunctionButton functionNodeId={id} />
                    </div>
                </div>
            </div>
        );
    }
}

export default FunctionNode;