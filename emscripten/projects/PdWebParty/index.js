////////////////////////////////////////////////////////////////////////////////
// server
////////////////////////////////////////////////////////////////////////////////
const http = require("http");
const https = require("https");
const path = require("path");
const express = require("express");
const bodyParser = require("body-parser");
const app = express();
const PORT = process.env.PORT || 3000;

// handle data in a nice way
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());

// static path
const publicPath = path.resolve(`${__dirname}/public`);
const emscriptenPath = path.resolve(`${publicPath}/emscripten`);
const URL = require("url").URL;

// set your static server
app.use(express.static(publicPath));
app.use(express.static(emscriptenPath));

// views
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "views/index.html"));
});

// api
app.get("/api/patch", async (req, res) => {
  const url = req.query.url;
  let client = http;
  if (url.toString().indexOf("https") === 0) {
    client = https;
  }
  let finalUrl = url;
  let valid = false;
  let error_type = null;
  try {
    //console.log("1: trying " + finalUrl);
    new URL(finalUrl);
    valid = true;
  } catch (err) {
    error_type = err;
    console.error("Pd-L2Ork Server: invalid URL, trying local file...");
  }
  if (!valid)
  {
    try {
      finalUrl = `http://localhost:${PORT}/` + url;
      //console.log("2: trying " + finalUrl);
      new URL(finalUrl);
      valid = true;
      console.log("Pd-L2Ork Server: local file successfully opened.")
    } catch (err2) {
      error_type = err2;
      console.error("Pd-L2Ork Server: invalid local file.");
    }
  }
  if (valid) {
    client.get(finalUrl, (_res) => {
      let chunks = [];
      // a chunk of data has been recieved.
      _res.on("data", (chunk) => {
        chunks.push(chunk);
      });
      // the whole response has been received.
      _res.on("end", () => {
        const buf = Buffer.concat(chunks);
        const content = buf.toString("utf-8");
        res.json({ content: content });
      });
    }).on("error", (err) => {
      res.json({ error: err });
    });
  } else {
    res.json({ error: error_type });
  }
});

// start listening
app.listen(PORT, () => {
  console.log(`Pd-L2Ork Server: running on localhost port ${PORT}`)
});
