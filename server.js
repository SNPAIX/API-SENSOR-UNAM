const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
app.use(cors());
app.use(express.json());

// Servir archivos estáticos (tu HTML, CSS, imágenes) desde la carpeta actual
app.use(express.static(__dirname));

// Objeto simulado con datos de prueba (por si quieres probar la API)
let sensorData = {
    t: 24.5,
    p: 1013.2,
    h: 55.0,
    H: [
        { t: 24.0, p: 1013.0, h: 54 },
        { t: 24.5, p: 1013.2, h: 55 }
    ],
    gps: { fix: true, lat: 19.4326, lon: -99.1332, alt: 2240, sats: 8, hora: "12:00:00" },
    trk: [[19.4326, -99.1332]]
};

// --- ENDPOINTS DE LA API ---

// 1. Obtener datos
app.get('/api/sensores', (req, res) => {
    res.json(sensorData);
});

// 2. Recibir datos externos (por si en el futuro quieres mandarlos por POST)
app.post('/api/sensores', (req, res) => {
    const nuevoDato = req.body;
    // Aquí podrías actualizar sensorData con lo que te manden
    res.json({ success: true, message: 'Datos recibidos correctamente' });
});

// 3. Limpiar registros
app.post('/api/limpiar', (req, res) => {
    sensorData.t = null;
    sensorData.p = null;
    sensorData.h = null;
    sensorData.H = [];
    sensorData.trk = [];
    sensorData.gps = { fix: false, lat: null, lon: null, alt: null, sats: 0, hora: null };
    res.json({ success: true, message: 'Datos reiniciados' });
});

const PORT = process.env.PORT || 8080;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`Servidor visible en la red local en: http://192.168.100.22:${PORT}`);
});