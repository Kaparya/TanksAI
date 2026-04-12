const Overlays = (() => {
  const menuEl       = document.getElementById('menu');
  const hudEl        = document.getElementById('hud');
  const pauseEl      = document.getElementById('pause-overlay');
  const gameoverEl   = document.getElementById('gameover');
  const waveClearEl  = document.getElementById('wave-clear');
  const minimapEl    = document.getElementById('minimap');

  function showMenu() {
    menuEl.classList.remove('hidden');
    hudEl.classList.remove('active');
    pauseEl.classList.remove('active');
    gameoverEl.classList.remove('active');
    waveClearEl.classList.remove('active');
    minimapEl.classList.remove('active');
  }

  function showGame() {
    menuEl.classList.add('hidden');
    hudEl.classList.add('active');
    minimapEl.classList.add('active');
    gameoverEl.classList.remove('active');
    waveClearEl.classList.remove('active');
    pauseEl.classList.remove('active');
  }

  function showPause() {
    pauseEl.classList.add('active');
  }

  function hidePause() {
    pauseEl.classList.remove('active');
  }

  function showGameOver(score, wave) {
    document.getElementById('go-score').textContent = score;
    document.getElementById('go-wave').textContent = wave;
    gameoverEl.classList.add('active');
  }

  function hideGameOver() {
    gameoverEl.classList.remove('active');
  }

  function isMenuVisible() {
    return !menuEl.classList.contains('hidden');
  }

  return { showMenu, showGame, showPause, hidePause, showGameOver, hideGameOver, isMenuVisible };
})();
