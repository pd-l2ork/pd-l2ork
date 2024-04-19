const http = require("http");
const https = require("https");
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
  let url = req.query.url;
  let client = url.toString().includes("https") ? https : http;
  try {
    url = new URL(url);
  } catch (err) {
    try {
      url = new URL(`http://localhost:${PORT}/` + url);
    } catch (err) {
      res.json({ error: 'Invalid Patch' });
      return;
    }
  }
  client.get(url, (_res) => {
    let chunks = [];
    // a chunk of data has been recieved.
    _res.on("data", (chunk) => {
      chunks.push(chunk);
    });
    // the whole response has been received.
    _res.on("end", () => {
      res.json({ content: Buffer.concat(chunks).toString("utf-8") });
    });
  }).on("error", (err) => {
    res.json({ error: err });
  });
})

// start listening
app.listen(PORT, () => {
  console.log(`Pd-L2Ork Server: running on localhost port ${PORT}`)
});
