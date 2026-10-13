import { useEffect } from 'react';

export default function CodeLine({id, line, key}) {

    return (
        <tr key={key}>
            <td> { line.no } </td>
            <td><pre><code className="language-cpp">{ line.content }</code></pre></td>
        </tr>
    )
}

