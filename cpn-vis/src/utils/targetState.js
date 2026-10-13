export const getTargetState = (transitionMap, currentState, transitionId) => {
    if (!transitionMap || !currentState || !transitionId) return null;
    return transitionMap[currentState]?.[transitionId];
};