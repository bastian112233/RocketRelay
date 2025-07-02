const TelegramBot = require('node-telegram-bot-api');
const mqtt = require('mqtt');

// ======== CONFIGURACIÓN =========
const TOKEN = 'TuToken';
const CHAT_ID = 'TuChatID'; 
const MQTT_BROKER = 'mqtt://test.mosquitto.org';
const client = mqtt.connect(MQTT_BROKER);
const bot = new TelegramBot(TOKEN, { polling: true });

// ======== VARIABLES =========
let ultimaTemp = null;
let ultimaHum = null;
let ultimaPres = null;
let ultimaGas = null;
let ultimaCorriente = null;
let ultimaIP = null;

// ======== MQTT ==========

client.on('connect', () => {
  console.log('📡 Conectado a MQTT');
  client.subscribe('home/temp');
  client.subscribe('home/humidity');
  client.subscribe('home/pressure');
  client.subscribe('home/gas');
  client.subscribe('home/current');
  client.subscribe('home/ip');
});

client.on('message', (topic, message) => {
  const valor = message.toString();
  switch (topic) {
    case 'home/temp':
      ultimaTemp = valor;
      break;
    case 'home/humidity':
      ultimaHum = valor;
      break;
    case 'home/pressure':
      ultimaPres = valor;
      break;
    case 'home/gas':
      ultimaGas = valor;
      break;
    case 'home/current':
      ultimaCorriente = valor;
      break;
    case 'home/ip':
      ultimaIP = valor;
      break;
  }
});

// ======== MENÚ PRINCIPAL =========

function enviarMenuPrincipal(chatId) {
  bot.sendMessage(chatId, '📋 *Menú principal:*', {
    parse_mode: 'Markdown',
    reply_markup: {
      inline_keyboard: [
	[{ text: '🔌 Controlar relés', callback_data: 'menu_reles' }],
        [{ text: '📊 Ver sensores', callback_data: 'ver_sensores' }],
        [{ text: '📶 Estado', callback_data: 'estado' }]
      ]
    }
  });
}

// ======== SUBMENÚ DE RELÉS =========

function enviarMenuReles(chatId) {
  bot.sendMessage(chatId, '🔌 *Control de relés:*', {
    parse_mode: 'Markdown',
    reply_markup: {
      inline_keyboard: [
        [
          { text: '🔴 Encender R1', callback_data: 'on_r1' },
          { text: '⚪ Apagar R1', callback_data: 'off_r1' }
        ],
        [
          { text: '🔴 Encender R2', callback_data: 'on_r2' },
          { text: '⚪ Apagar R2', callback_data: 'off_r2' }
        ],
        [
          { text: '🔴 Encender R3', callback_data: 'on_r3' },
          { text: '⚪ Apagar R3', callback_data: 'off_r3' }
        ],
        [
          { text: '🔴 Encender R4', callback_data: 'on_r4' },
          { text: '⚪ Apagar R4', callback_data: 'off_r4' }
        ],
        [
          { text: '⬅️ Volver', callback_data: 'volver_menu' }
        ]
      ]
    }
  });
}

// ======== COMANDOS =========

bot.onText(/\/start/, (msg) => {
  enviarMenuPrincipal(msg.chat.id);
});

bot.on('callback_query', async (query) => {
  const chatId = query.message.chat.id;
  const data = query.data;

  if (data === 'ver_sensores') {
    const mensaje = `📊 *Datos de sensores:*\n` +
      (ultimaTemp ? `🌡️ Temp: *${ultimaTemp}°C*\n` : '') +
      (ultimaHum ? `💧 Humedad: *${ultimaHum}%*\n` : '') +
      (ultimaPres ? `📈 Presión: *${ultimaPres} hPa*\n` : '') +
      (ultimaGas ? `🧪 Gas: *${ultimaGas} kΩ*\n` : '') +
      (ultimaCorriente ? `⚡ Corriente: *${ultimaCorriente} A*\n` : '') +
      `_Actualizado cada pocos segundos_`;

    bot.sendMessage(chatId, mensaje, { parse_mode: 'Markdown' });

  } else if (data === "estado") {
    
    const wifiStatus = "✅ WiFi: Conectado";
    const mqttStatus = client.connected
      ? "✅ MQTT: Conectado a test.mosquitto.org"
      : "❌ MQTT: No conectado";

    const estado = `📡 Estado del sistema:\n${wifiStatus}\n${mqttStatus}\n📈 Todo funcionando correctamente 🚀`;

    await bot.answerCallbackQuery(query.id); // responder sin mensaje emergente
    bot.sendMessage(chatId, estado, { parse_mode: 'Markdown' });
    enviarMenuPrincipal(chatId);

  } else if (data === 'menu_reles') {
    enviarMenuReles(chatId);

  } else if (data.startsWith('on_') || data.startsWith('off_')) {
    const releNum = data.slice(-1);
    const estado = data.startsWith('on_') ? 'ON' : 'OFF';
    client.publish(`home/relay${releNum}`, estado);
    bot.sendMessage(chatId, `✅ Relé ${releNum} ${estado === 'ON' ? 'encendido' : 'apagado'}`);

  } else if (data === 'volver_menu') {
    enviarMenuPrincipal(chatId);
  }

  // Confirma que se recibió la interacción
  bot.answerCallbackQuery(query.id);
});