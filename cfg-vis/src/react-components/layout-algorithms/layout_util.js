
// berechne Knotendimensionen basierend auf den Codezeilen im Textfeld
// ermöglicht ein präziseres Layout schon beim ersten Render
export function getNodeDimension(node) {
    if (node.width && node.height) {
        return { width: node.width, height: node.height };
    }

    // Basis-Dimensionen
    const PADDING = 20;         // Padding des Knotens (2 × 10px)
    const LABEL_HEIGHT = 16;    // Höhe des Labels (mit margin)
    const HANDLES = 8;          // Höhe der Handles
    const CHAR_WIDTH = 7;       // Monospace Zeichenbreite in px (7pt Font)
    const LINE_HEIGHT = 14;     // CSS: max-height: 14px für pre-Tags
    const LINE_NUMBER_WIDTH = 30; // Breite der Zeilennummern-Spalte
    
    // bei Funktionsknoten nur Standardgröße möglich
    if (node.type === "function-node") {
        return { width: 150, height: 60 };
    }
    
    // cfg-nodes: Dimensionen aus Codezeilen berechnen
    if (node.data?.text && Array.isArray(node.data.text) && node.data.text.length > 0) {
        const numLines = node.data.text.length;

        // Scrollbox wenn zu viele Zeilen in einem Knoten
        const MAX_VISIBLE_LINES = 5.5;
        const visibleLines = Math.min(numLines, MAX_VISIBLE_LINES);
        const height = visibleLines * LINE_HEIGHT + LABEL_HEIGHT + PADDING + HANDLES;
        
        
        const longestLine = Math.max(
            node.data.label?.length || 0,
            ...node.data.text.map((line) => {
                const content = line.content || '';
                return content.length;
            })
        );
        
        const width = longestLine * CHAR_WIDTH + LINE_NUMBER_WIDTH + PADDING;
        
        return { 
            width: Math.max(width, 80), // Minimum Breite
            height: Math.max(height, 50) // Minimum Höhe
        };
    }
    
    // Fallback für Knoten ohne Codezeilen (nur Label wie "main0", "main5")
    // WICHTIG: Breite MUSS genau mit CSS übereinstimmen sonst verbinden Kanten sich nicht korrekt!
    // CSS in nodes.css: .cfg-node { padding: 10px } = 20px PADDING
    const labelLength = node.data?.label?.length || 4;
    // Die echte CSS-Breite ergibt sich aus: Label-Breite + Padding
    const width = labelLength * CHAR_WIDTH + PADDING;
    
    return { 
        width: Math.max(width, 50), // Minimum 50px für kleinste Labels
        height: 40
    };
}