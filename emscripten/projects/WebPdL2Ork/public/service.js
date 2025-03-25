let nextMessageId = 0;
const clients = {};
const waiters = [];
const getChildren = parent => Object.values(clients).filter(client => client.parent?.id === parent);
const waitForMessage = (id, data) => new Promise(Resolve => {
    const messageId = nextMessageId++;
    waiters.push({ validator: message => message.messageId === messageId, handler: Resolve });
    clients[id].postMessage({ messageId, ...data });
});

addEventListener('install', () => {
    self.skipWaiting();
});

addEventListener('activate', () => {
    self.clients.claim();
    self.clients.matchAll({ includeUncontrolled: true, type: 'window'}).then(foundClients => {
        for(const client of foundClients)
            client.postMessage({ type: 'reregister'});
    });
});

addEventListener('message', async(event) => {
    const source = event.source.id;
    const data = event.data;
    if(data.type === 'register') {
        clients[source] = event.source;
        setTimeout(() => {
            if(data.parent)
                event.source.parent = clients[data.parent];
        }, 100);
        event.source.postMessage({ type: 'register', data: { id: source, nextPatchID: data.parent ? await waitForMessage(data.parent, { type: 'patchID' }) : 1003}});
    } else if(data.type === 'children') {
        getChildren(source).forEach(child => child.postMessage(data.data));
    } else if(data.type === 'parent') {
        clients[source]?.parent?.postMessage?.(data.data);
    } else if(data.type === 'unload') {
        clients[source]?.parent?.postMessage?.({ type: 'unload', data: source });
        getChildren(source).forEach(child => child.postMessage({ type: 'unload' }));
        delete clients[source];
    }
    else
        waiters.find(waiter => waiter.validator(data))?.handler?.(data.result);
});

addEventListener('fetch', e => {
    const url = new URL(e.request.url);
    if(url.pathname.startsWith('/@sw')) {
        const args = Object.fromEntries(new URLSearchParams(url.search).entries());
        e.respondWith((async() => new Response(JSON.stringify(await waitForMessage(args.pageId, args))))())
    }
})