export const parseHtmlLabel = (htmlLabel) => {
    if (!htmlLabel) return '';

    return htmlLabel
        .replace(/<B>/g, '<strong>')
        .replace(/<\/B>/g, '</strong>')
        .replace(/<BR\/>/g, '<br/>')
        .replace(/<I>/g, '<em>')
        .replace(/<\/I>/g, '</em>')
        .replace(/<U>/g, '<u>')
        .replace(/<\/U>/g, '</u>');
};

