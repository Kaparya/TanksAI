// ═══════════════════════════════════════════════════
//  WebSocket client — connects to C++ game server
// ═══════════════════════════════════════════════════

const Network = (() => {
  let ws = null;
  let connected = false;
  let onStateCallback = null;
  let reconnectTimer = null;
  let myPlayerId = null;

  const WS_PORT = 9001; // server listens WS on port+1

  function getWsUrl() {
    const host = window.location.hostname || 'localhost';
    return `ws://${host}:${WS_PORT}`;
  }

  function connect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

    const url = getWsUrl();
    console.log('[Network] Connecting to', url);
    UI.setConnectionStatus('connecting');

    ws = new WebSocket(url);

    ws.onopen = () => {
      connected = true;
      console.log('[Network] Connected');
      UI.setConnectionStatus('connected');
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'welcome') {
          myPlayerId = data.playerId;
          console.log('[Network] Assigned player ID:', myPlayerId);
          return;
        }
        if (data.type === 'full') {
          console.warn('[Network] Server full');
          return;
        }
        if (onStateCallback) onStateCallback(data);
      } catch (e) {
        console.warn('[Network] Bad message:', e);
      }
    };

    ws.onclose = () => {
      connected = false;
      myPlayerId = null;
      console.log('[Network] Disconnected');
      UI.setConnectionStatus('disconnected');
      scheduleReconnect();
    };

    ws.onerror = () => {
      UI.setConnectionStatus('disconnected');
    };
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connect();
    }, 2000);
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }

  function sendInput(keys) {
    send({ type: 'input', keys });
  }

  function sendStart(hardmode) {
    send({ type: 'start', hardmode });
  }

  function sendPause()   { send({ type: 'pause' }); }
  function sendResume()  { send({ type: 'resume' }); }
  function sendRestart() { send({ type: 'restart' }); }
  function sendQuit()    { send({ type: 'quit' }); }
  function sendBuy(upgrade) { send({ type: 'buy', upgrade }); }

  function onState(cb) { onStateCallback = cb; }
  function isConnected() { return connected; }
  function getMyPlayerId() { return myPlayerId; }

  return {
    connect, send, sendInput, sendStart,
    sendPause, sendResume, sendRestart, sendQuit, sendBuy,
    onState, isConnected, getMyPlayerId
  };
})();
