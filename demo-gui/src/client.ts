import init from './ClientRoot';

let prev = init(null);
if ((module as any).hot)
    (module as any).hot.accept('./ClientRoot', () => { prev = init(prev); });
