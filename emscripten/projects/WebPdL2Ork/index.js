const axios = require("axios");
const express = require("express");
const bodyParser = require("body-parser");
const app = express();
const PORT = process.env.PORT || 3000;
const PATCH_PATH = process.env.PATCH_PATH || 'public';

// Body parsing for GET paramaters
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());

// File search paths
app.use(express.static("public"));
app.use(express.static("public/emscripten"));
app.use(express.static(PATCH_PATH));

// Main views
app.get("/", (req, res) => {
  res.sendFile(`${__dirname}/views/index.html`);
});

// Fetching patches
app.get("/api/patch", async (req, res) => {
  let urls = req.query.url;
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

app.get("/api/file", async (req, res) => {

  let urls = req.query.url;
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

// start listening
app.listen(PORT, () => {
  console.log(`Pd-L2Ork Server: running on localhost port ${PORT}`)
});
