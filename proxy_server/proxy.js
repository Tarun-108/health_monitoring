const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');

const app = express();

// This proxy will forward requests from "/sensor" to the HTTPS server.
const sensorProxy = createProxyMiddleware({
  target: 'https://healthmonitoring-production.up.railway.app',
  changeOrigin: true,
  secure: true,
  // Optionally, if you need to rewrite the path, uncomment the next line:
  // pathRewrite: { '^/sensor': '' },
});

// Use the proxy middleware for all routes under /sensor.
app.use('/', sensorProxy);

// Start the proxy server on port 8080 (or whichever port you choose).
app.listen(8080, () => {
  console.log('Proxy server running at http://localhost:8080');
});
