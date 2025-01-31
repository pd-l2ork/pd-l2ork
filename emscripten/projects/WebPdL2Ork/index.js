const axios = require("axios");
const express = require("express");
const ws = require("ws");
const net = require('net')
const compression = require("compression");
const app = express();
const PORT = process.env.PORT || 3000;
const PATCH_PATH = process.env.PATCH_PATH || 'public';
const STATIC_OPTIONS = {
  maxAge: process.env.DISABLE_CACHE ? undefined : '7d',
};

// Body parsing for JSON
app.use(express.json());
app.use(compression({ filter: () => true }));

// File search paths
// While it may seem like a good idea to change the order of these to have better prioritization of files (ie bind mount patches vs
// baked patches, this completely breaks everything...)
app.use(express.static("public", STATIC_OPTIONS));
app.use(express.static("public/emscripten", STATIC_OPTIONS));
app.use(express.static(PATCH_PATH, STATIC_OPTIONS));

// Main views
app.get("/", (req, res) => {
  res.sendFile(`${__dirname}/views/index.html`);
});

// Fetching patches
app.post("/api/patch", async (req, res) => {
  let urls = req.body.urls;
  if(typeof urls === 'string')
    urls = [urls];

  res.json(Object.fromEntries(await Promise.all(urls.map(url => new Promise(Resolve => {
    let purl;
    try {
      purl = new URL(url);
    } catch (err) {
      try {
        purl = new URL(`http://localhost:${PORT}/` + url);
      } catch (err) {
        res.json({ error: 'Invalid Patch' });
        return;
      }
    }
    axios.get(purl).then(response => {
      Resolve([url, response.data]);
    }).catch(error => {
      Resolve([url, '']);
    });
  })))));
});

app.post("/api/file", async (req, res) => {
  let urls = req.body.urls;
  if(typeof urls === 'string')
    urls = [urls];

  const result = (await Promise.all(urls.map(url => new Promise(Resolve => {
    let purl;
    try {
      purl = new URL(url);
    } catch (err) {
      try {
        purl = new URL(`http://localhost:${PORT}/` + url);
      } catch (err) {
        res.json({ error: 'Invalid Patch' });
        return;
      }
    }
    axios({
      method: 'GET',
      url: purl.href, 
      responseType: 'arraybuffer',
    }).then(response => {
      Resolve(response.data);
    }).catch(error => {
      Resolve(null);
    });
  })))).find(result => result !== null);

  if(result)
    res.send(result);
  else
    res.sendStatus(204);
});

// Websocket server to proxy network requests. All network connections
// from WebPdL2Ork will be sent to this server, along with a message
// containing the host to connect to. We then proxy all data between
// the user and that host.
const wsServer = new ws.Server({ noServer: true });
wsServer.on('connection', client => {
  let target; // The host to proxy
  
  client.on('message', msg => {
    // If we haven't set up the connection yet, set it up
    if (!target) {
      // Determine hostname and port from original connection url that was intercepted
      const [_, host, port] = msg.toString().match(/^(?:wss?:\/\/)?([^:/]+)(?::(\d+))?/);
      if (!host || !port) // First message must contain a hostname and port
        return client.close();

      // Connect to the remote host and set up data proxy
      target = net.createConnection(port, host);
      target.on('data', data => {
        try {
          client.send(data);
        } catch (e) {
          target.end();
        }
      });
      target.on('end', () => client.close());
      target.on('error', () => {
        target.end();
        client.close();
      });
    }
    else // Otherwise, proxy all data
      target.write(msg);
  });
  client.on('close', () => target?.end?.());
  client.on('error', () => target?.end?.());
});

// Start listening
const server = app.listen(PORT, () => {
  console.log(`Pd-L2Ork Server: running on localhost port ${PORT}`)
});

// Connect to websocket server
server.on('upgrade', (request, socket, head) => {
  wsServer.handleUpgrade(request, socket, head, socket => {
    wsServer.emit('connection', socket, request);
  });
});
