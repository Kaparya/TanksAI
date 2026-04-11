// ═══════════════════════════════════════════════════
//  App — main entry point, ties everything together
// ═══════════════════════════════════════════════════

const App = (() => {
  let currentState = 'menu'; // tracks local UI state
  let lastServerState = null;

  function init() {
    // Connect to server
    Network.connect();

    // Handle incoming state from server
    Network.onState((state) => {
      lastServerState = state;

      if (state.type === 'state') {
        handleState(state);
      }
    });

    // Button handlers
    document.getElementById('btn-play').addEventListener('click', () => startGame(false));
    document.getElementById('btn-hard').addEventListener('click', () => startGame(true));
    document.getElementById('btn-resume').addEventListener('click', () => resumeGame());
    document.getElementById('btn-quit').addEventListener('click', () => quitToMenu());
    document.getElementById('btn-restart').addEventListener('click', () => restartGame());
    document.getElementById('btn-menu').addEventListener('click', () => quitToMenu());

    // Start render loop
    requestAnimationFrame(loop);
  }

  function startGame(hardmode) {
    if (!Network.isConnected()) {
      console.warn('Not connected to server');
      return;
    }
    Network.sendStart(hardmode);
    UI.showGame();
    currentState = 'playing';
  }

  function togglePause() {
    if (currentState === 'playing') {
      Network.sendPause();
      UI.showPause();
      currentState = 'paused';
    } else if (currentState === 'paused') {
      resumeGame();
    }
  }

  function resumeGame() {
    Network.sendResume();
    UI.hidePause();
    currentState = 'playing';
  }

  function quitToMenu() {
    Network.sendQuit();
    UI.showMenu();
    currentState = 'menu';
    Renderer.clear();
  }

  function restartGame() {
    Network.sendRestart();
    UI.hideGameOver();
    currentState = 'playing';
  }

  function handleState(state) {
    // Auto-join if game is already running (player 2 joining mid-game)
    if ((state.gameState === 'playing' || state.gameState === 'wave_clear') && currentState === 'menu') {
      UI.showGame();
      currentState = state.gameState === 'wave_clear' ? 'wave_clear' : 'playing';
    }

    // Update HUD
    if (state.gameState === 'playing' || state.gameState === 'paused' || state.gameState === 'wave_clear') {
      UI.updateHUD(state);
    }

    // Detect wave clear from server
    if (state.gameState === 'wave_clear' && currentState !== 'wave_clear') {
      currentState = 'wave_clear';
      UI.showWaveClear(state);
    }
    if (state.gameState === 'playing' && currentState === 'wave_clear') {
      currentState = 'playing';
      UI.hideWaveClear();
    }

    // Detect game over from server
    if (state.gameState === 'gameover' && currentState !== 'gameover') {
      currentState = 'gameover';
      UI.showGameOver(state.score, state.wave);
    }

    // Detect pause from server
    if (state.gameState === 'paused' && currentState !== 'paused') {
      currentState = 'paused';
      UI.showPause();
    }

    // Render
    Renderer.draw(state);
  }

  // ── Input sending loop ──
  let inputSendInterval = null;

  function startInputLoop() {
    if (inputSendInterval) return;
    inputSendInterval = setInterval(() => {
      if (currentState === 'playing' && Network.isConnected()) {
        Network.sendInput(Input.getState());
      }
    }, 16); // ~60 FPS
  }

  // ── Render loop ──
  function loop() {
    if (currentState === 'menu') {
      UI.drawMenuBg();
    }
    // Send input only when keys change
    if (currentState === 'playing' && Network.isConnected() && Input.isDirty()) {
      Network.sendInput(Input.getState());
      Input.clearDirty();
    }
    requestAnimationFrame(loop);
  }

  // Boot
  document.addEventListener('DOMContentLoaded', init);

  return { togglePause };
})();
