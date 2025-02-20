let nextMessageId = 0;
const waiters = [];
const getClient = async(id) => (await self.clients.matchAll({includeUncontrolled: true, type: 'window'})).find(client => client.id === id);
const postClient = (id, data) => getClient(id).then(client => client.postMessage(data));
const waitForMessage = (id, data) => new Promise(Resolve => {
    const messageId = nextMessageId++;
    waiters.push({ validator: message => message.messageId === messageId, handler: Resolve });
    postClient(id, { messageId, ...data });
});

addEventListener('install', () => {
    self.skipWaiting();
});

addEventListener('activate', () => {
    self.clients.claim();
});

addEventListener('message', async(event) => {
    const source = event.source.id;
    const data = event.data;
    if(data.type === 'register')
        postClient(source, { type: 'register', data: source });
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