/* 

Hook that is run when the page loads to find the patch to load.
It is given as an input the value of the ?url=xyz search parameter.
This parameter may be respected or may be overridden according to the
needs of the page. The function must return an object of the following
structure:

{
    patchURL: 'path/to/some/patch.pd',

    OR

    content: '#N canvas ...',
    filename: 'patchName.pd',
}

*/
async function patchHook(url) {
    return {
        patchURL: 'demo.pd',
    };
}

/* Hook that is run when any patch is successfully loaded. */
async function initHook() {
    /* addDataHook allows subscribing to wireless names to receive the data
       sent along them. The first parameter is an object containing event 
       functions onAny, onBang, onFloat, onSymbol, onList, and onMessage.
       The second parameter is a list of wireless names to listen on. */
    addDataHook({
        onSymbol: (source, data) => {
            document.getElementById('output').innerHTML = `Current value of ${source} is: "${data}"`;
        }
    }, ['text_out']);
}

async function randomize() {
    /* gui_send allows sending data to PdL2Ork over a wireless connection.
       The first parameter is the type of data (Bang, Float, List, etc.),
       the second parameter is the list of wireless names to send it to,
       and the third parameter is the data to send. */ 
    gui_send('Float', ['random_in'], Math.random());
}