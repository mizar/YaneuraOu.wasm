#!/usr/bin/env node
const express = require('express');
const expressStaticGzip = require('express-static-gzip');
const app = express();
const cors = function(req, res, next) {
  res.header('Access-Control-Allow-Origin', '*')
  res.header('Cross-Origin-Embedder-Policy', 'require-corp');
  res.header('Cross-Origin-Opener-Policy', 'same-origin');

  // intercept OPTIONS method
  if ('OPTIONS' === req.method) {
    res.send(200)
  } else {
    next()
  }
}
const port = 3000;

app.use(cors)
app.use("/", expressStaticGzip("./public_wasm/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/halfkp/", expressStaticGzip("./build/wasm/halfkp/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/halfkp.noeval/", expressStaticGzip("./build/wasm/halfkp.noeval/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/halfkpe9.noeval/", expressStaticGzip("./build/wasm/halfkpe9.noeval/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/halfkpvm.noeval/", expressStaticGzip("./build/wasm/halfkpvm.noeval/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/k-p/", expressStaticGzip("./build/wasm/k-p/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/k-p.noeval/", expressStaticGzip("./build/wasm/k-p.noeval/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/material/", expressStaticGzip("./build/wasm/material/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/material9/", expressStaticGzip("./build/wasm/material9/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/tanuki-mate/", expressStaticGzip("./build/wasm/tanuki-mate/", { enableBrotli: true, orderPreference: ['br'] }));
app.use("/yaneuraou-mate/", expressStaticGzip("./build/wasm/yaneuraou-mate/", { enableBrotli: true, orderPreference: ['br'] }));

app.listen(port, () => {
  console.log(`listening at http://localhost:${port}`);
});
