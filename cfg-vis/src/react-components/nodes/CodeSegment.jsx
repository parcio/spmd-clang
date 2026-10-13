import PropTypes from 'prop-types';
import ExtendFunctionButton from './ExtendFunctionButton';
import CodeLine from './CodeLine';

function CodeSegment({ id, text }) {

    let tableClass = "normal";
    if(text.length > 5) { tableClass = "scroll"; }

    return (
        <div>
            { text.length > 0 && 
                <table>
                    <tbody className={tableClass}>
                        {text.map((line, key) => {
                            return <CodeLine id={id} line={line} key={key}/>
                        })}
                    </tbody>
                </table>
            }
        </div>
    );

}

CodeSegment.propTypes = {
    text : PropTypes.arrayOf(PropTypes.shape({
        no : PropTypes.number.isRequired,
        content : PropTypes.string,
        functionCall : PropTypes.bool,
        functionNode : PropTypes.string
        }
    ))
}
// 
export default CodeSegment;