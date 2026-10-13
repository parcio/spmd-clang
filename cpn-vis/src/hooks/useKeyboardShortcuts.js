import { useEffect } from 'react';
import { useAtomValue, useSetAtom } from 'jotai';
import { showClickHelpAtom } from '../atoms/petriNetAtoms';

export const useKeyboardShortcuts = (onNext, fireAnimation) => {
    const setShowClickHelp = useSetAtom(showClickHelpAtom);

    useEffect(() => {
        const handleKeyDown = (event) => {
            if (event.code === 'Space' && !fireAnimation) {
                event.preventDefault();
                onNext();
            }

            if (event.code === 'Escape') {
                setShowClickHelp(false);
            }
        };

        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [onNext, fireAnimation, setShowClickHelp]);
};