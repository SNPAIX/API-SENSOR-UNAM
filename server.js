const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
// Escuchar en el puerto 3000 por defecto
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(__dirname));

let sensorData = {
  t: null, p: null, h: null,
  H: [],
  gps: { fix: false, lat: null, lon: null, alt: null, sats: 0, hora: null },
  trk: []
};

// Endpoint para recibir datos del ESP32
app.post('/api/sensores', (req, res) => {
  const data = req.body;
  if (data) {
    sensorData.t = data.t;
    sensorData.p = data.p;
    sensorData.h = data.h;
    
    sensorData.H.push({ t: data.t, p: data.p, h: data.h });
    if (sensorData.H.length > 120) sensorData.H.shift();

    if (data.gps) {
      sensorData.gps = data.gps;
      if (data.gps.fix && data.gps.lat && data.gps.lon) {
        sensorData.trk.push([data.gps.lat, data.gps.lon]);
      }
    }
    console.log("Datos recibidos:", data);
    res.status(200).json({ message: 'Datos guardados' });
  } else {
    res.status(400).json({ error: 'Datos no válidos' });
  }
});

// Endpoint para entregar datos al Panel
app.get('/api/sensores', (req, res) => {
  res.json(sensorData);
});

// Endpoint para limpiar historial
app.post('/api/limpiar', (req, res) => {
  sensorData.H = [];
  sensorData.trk = [];
  res.status(200).json({ message: 'Historial limpiado' });
});

// El servidor escucha en TODAS las interfaces (0.0.0.0) para recibir tráfico externo
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Servidor de la UNAM en línea: http://132.248.26.212:${PORT}`);
});
