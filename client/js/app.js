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
    Overlays.showGame();
    currentState = 'playing';
  }

  function togglePause() {
    if (currentState === 'playing') {
      Network.sendPause();
      Overlays.showPause();
      currentState = 'paused';
    } else if (currentState === 'paused') {
      resumeGame();
    }
  }

  function resumeGame() {
    Network.sendResume();
    Overlays.hidePause();
    currentState = 'playing';
  }

  function quitToMenu() {
    Network.sendQuit();
    Overlays.showMenu();
    currentState = 'menu';
    Renderer.clear();
  }

  function restartGame() {
    Network.sendRestart();
    Overlays.hideGameOver();
    currentState = 'playing';
  }

  function handleState(state) {
    // Auto-join if game is already running (player 2 joining mid-game)
    if ((state.gameState === 'playing' || state.gameState === 'wave_clear') && currentState === 'menu') {
      Overlays.showGame();
      currentState = state.gameState === 'wave_clear' ? 'wave_clear' : 'playing';
    }

    // Update HUD
    if (state.gameState === 'playing' || state.gameState === 'paused' || state.gameState === 'wave_clear') {
      HUD.updateHUD(state);
    }

    // Detect wave clear from server
    if (state.gameState === 'wave_clear' && currentState !== 'wave_clear') {
      currentState = 'wave_clear';
      Shop.showWaveClear(state);
    }
    if (state.gameState === 'wave_clear' && currentState === 'wave_clear') {
      Shop.updateWaveClear(state);
    }
    if (state.gameState === 'playing' && currentState === 'wave_clear') {
      currentState = 'playing';
      Shop.hideWaveClear();
    }

    // Detect game over from server
    if (state.gameState === 'gameover' && currentState !== 'gameover') {
      currentState = 'gameover';
      Overlays.showGameOver(state.score, state.wave);
    }

    // Detect pause from server
    if (state.gameState === 'paused' && currentState !== 'paused') {
      currentState = 'paused';
      Overlays.showPause();
    }

    // Render
    Renderer.draw(state);
  }

  // ── Render loop ──
  function loop() {
    if (currentState === 'menu') {
      MenuBg.drawMenuBg();
    }
    // Send input each frame if playing
    if (currentState === 'playing' && Network.isConnected()) {
      Network.sendInput(Input.getState());
    }
    requestAnimationFrame(loop);
  }

  // Boot
  document.addEventListener('DOMContentLoaded', init);

  return { togglePause };
})();
