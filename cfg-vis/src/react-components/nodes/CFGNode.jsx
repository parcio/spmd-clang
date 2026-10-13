import { Handle } from "reactflow";
import PropTypes from 'prop-types';
import CodeSegment from "./CodeSegment";


function CFGNode( {id, data} ) {

    return (
        <>
        <div className="cfg-node">
            <Handle type="target" />
            <div>
                <label htmlFor="name"><b>{ data.label }</b></label>
                { data.text?.length !== 0 && 
                    <div>
                        <CodeSegment id={id} text={data.text} />
                    </div>
                }
            </div>
            <Handle type="source"/>
        </div>
        </>
    );
}

CFGNode.propTypes = {
    data : PropTypes.shape({
        label : PropTypes.string.isRequired,
        text : PropTypes.array
    }) 
};

export default CFGNode;