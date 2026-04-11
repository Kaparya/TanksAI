// ═══════════════════════════════════════════════════
//  UI — menus, HUD, overlays
// ═══════════════════════════════════════════════════

const UI = (() => {
  const menuEl       = document.getElementById('menu');
  const hudEl        = document.getElementById('hud');
  const pauseEl      = document.getElementById('pause-overlay');
  const gameoverEl   = document.getElementById('gameover');
  const waveClearEl  = document.getElementById('wave-clear');
  const minimapEl    = document.getElementById('minimap');
  const connDot      = document.getElementById('conn-dot');
  const connText     = document.getElementById('conn-text');

  // Menu background animation
  const bgCanvas = document.getElementById('bgCanvas');
  const bgCtx = bgCanvas.getContext('2d');
  bgCanvas.width = 400; bgCanvas.height = 300;

  const bgParticles = [];
  for (let i = 0; i < 60; i++) {
    bgParticles.push({
      x: Math.random() * 400, y: Math.random() * 300,
      vx: (Math.random() - .5) * .5, vy: (Math.random() - .5) * .5,
      r: 1 + Math.random() * 2, a: .2 + Math.random() * .4
    });
  }

  function drawMenuBg() {
    bgCtx.clearRect(0, 0, 400, 300);
    for (const p of bgParticles) {
      p.x += p.vx; p.y += p.vy;
      if (p.x < 0) p.x = 400; if (p.x > 400) p.x = 0;
      if (p.y < 0) p.y = 300; if (p.y > 300) p.y = 0;
      bgCtx.globalAlpha = p.a;
      bgCtx.fillStyle = '#00e5ff';
      bgCtx.beginPath(); bgCtx.arc(p.x, p.y, p.r, 0, Math.PI * 2); bgCtx.fill();
    }
    bgCtx.strokeStyle = '#00e5ff'; bgCtx.lineWidth = .3;
    for (let i = 0; i < bgParticles.length; i++) {
      for (let j = i + 1; j < bgParticles.length; j++) {
        const dx = bgParticles[i].x - bgParticles[j].x;
        const dy = bgParticles[i].y - bgParticles[j].y;
        const d = Math.sqrt(dx * dx + dy * dy);
        if (d < 80) {
          bgCtx.globalAlpha = (1 - d / 80) * .15;
          bgCtx.beginPath();
          bgCtx.moveTo(bgParticles[i].x, bgParticles[i].y);
          bgCtx.lineTo(bgParticles[j].x, bgParticles[j].y);
          bgCtx.stroke();
        }
      }
    }
    bgCtx.globalAlpha = 1;
  }

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

  function showWaveClear(state) {
    document.getElementById('wc-wave-num').textContent = 'Wave ' + state.wave + ' Complete';
    const container = document.getElementById('wc-players');
    container.innerHTML = '';
    for (const p of (state.players || [])) {
      const card = document.createElement('div');
      card.className = 'wc-player-card';
      const color = p.color || '#fff';
      card.innerHTML =
        '<div class="wc-player-name" style="color:' + color + '">Player ' + (p.id + 1) + '</div>' +
        '<div class="wc-money-label">Total Money</div>' +
        '<div class="wc-money-value">$' + (p.money || 0) + '</div>';
      container.appendChild(card);
    }
    waveClearEl.classList.add('active');
  }

  function hideWaveClear() {
    waveClearEl.classList.remove('active');
  }

  // Cache HUD DOM refs and previous values to avoid unnecessary DOM writes
  const hudScoreEl = document.getElementById('val-score');
  const hudWaveEl = document.getElementById('val-wave');
  const hudEnemiesEl = document.getElementById('val-enemies');
  const hudLivesEl = document.getElementById('val-lives');
  const hudPlayersEl = document.getElementById('val-players');
  let prevScore = -1, prevWave = -1, prevEnemies = -1, prevLives = -1, prevPlayerCount = -1;

  function updateHUD(state) {
    if (state.score !== prevScore) {
      hudScoreEl.textContent = state.score;
      prevScore = state.score;
    }
    if (state.wave !== prevWave) {
      hudWaveEl.textContent = state.wave;
      prevWave = state.wave;
    }

    let aliveCount = 0;
    for (const e of state.enemies) if (e.alive) aliveCount++;
    const remaining = aliveCount + state.enemiesLeft;
    if (remaining !== prevEnemies) {
      hudEnemiesEl.textContent = remaining;
      prevEnemies = remaining;
    }

    const myId = Network.getMyPlayerId();
    let myLives = 0;
    if (state.players) {
      for (const p of state.players) {
        if (p.id === myId) { myLives = p.lives; break; }
      }
    }
    if (myLives !== prevLives) {
      hudLivesEl.textContent = myLives;
      prevLives = myLives;
    }

    if (state.players && state.players.length !== prevPlayerCount) {
      hudPlayersEl.textContent = state.players.length;
      prevPlayerCount = state.players.length;
    }
  }

  function setConnectionStatus(status) {
    connDot.className = '';
    if (status === 'connected') {
      connDot.classList.add('connected');
      connText.textContent = 'Connected';
    } else if (status === 'connecting') {
      connDot.classList.add('connecting');
      connText.textContent = 'Connecting...';
    } else {
      connText.textContent = 'Disconnected';
    }
  }

  function isMenuVisible() {
    return !menuEl.classList.contains('hidden');
  }

  return {
    drawMenuBg, showMenu, showGame,
    showPause, hidePause,
    showGameOver, hideGameOver,
    showWaveClear, hideWaveClear,
    updateHUD, setConnectionStatus, isMenuVisible
  };
})();
