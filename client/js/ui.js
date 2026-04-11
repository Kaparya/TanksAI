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

  const UPGRADE_COSTS = {
    damage: [0, 20, 50],   // cost to go from level i to i+1 (index = current level)
    size:   [0, 15, 35],
  };

  function levelStars(level, max) {
    return '★'.repeat(level) + '☆'.repeat(max - level);
  }

  function showWaveClear(state) {
    document.getElementById('wc-wave-num').textContent = 'Wave ' + state.wave + ' Complete';

    // Player money cards
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

    // Shop (only for this client's player)
    const shopEl = document.getElementById('wc-shop');
    shopEl.innerHTML =
      '<div class="wc-shop-title">UPGRADES</div>' +
      '<div class="wc-shop-balance">Balance: <span id="wc-balance">$0</span></div>' +
      '<div class="wc-shop-items">' +
        '<div class="wc-shop-item">' +
          '<div class="wc-item-icon">💥</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Bullet Damage</div>' +
            '<div class="wc-item-stars" id="wc-dmg-stars">★☆☆</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-dmg-cost">$20</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-damage">Buy</button>' +
        '</div>' +
        '<div class="wc-shop-item">' +
          '<div class="wc-item-icon">⭕</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Bullet Size</div>' +
            '<div class="wc-item-stars" id="wc-size-stars">★☆☆</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-size-cost">$15</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-size">Buy</button>' +
        '</div>' +
      '</div>';

    document.getElementById('wc-btn-damage').addEventListener('click', () => Network.sendBuy('damage'));
    document.getElementById('wc-btn-size').addEventListener('click', () => Network.sendBuy('size'));

    updateWaveClear(state);
    waveClearEl.classList.add('active');
  }

  function updateWaveClear(state) {
    const shopEl = document.getElementById('wc-shop');
    if (!shopEl || !shopEl.children.length) return;

    const myId = Network.getMyPlayerId();
    const me = state.players ? state.players.find(p => p.id === myId) : null;
    if (!me) return;

    const money = me.money || 0;
    document.getElementById('wc-balance').textContent = '$' + money;

    // Damage upgrade
    const dmgLevel = me.bulletDmgLevel || 1;
    const dmgMaxed = dmgLevel >= 3;
    const dmgCost = UPGRADE_COSTS.damage[dmgLevel] || 0;
    document.getElementById('wc-dmg-stars').textContent = levelStars(dmgLevel, 3);
    document.getElementById('wc-dmg-cost').textContent = dmgMaxed ? 'MAX' : '$' + dmgCost;
    const dmgBtn = document.getElementById('wc-btn-damage');
    if (dmgBtn) dmgBtn.disabled = dmgMaxed || money < dmgCost;

    // Size upgrade
    const sizeLevel = me.bulletSizeLevel || 1;
    const sizeMaxed = sizeLevel >= 3;
    const sizeCost = UPGRADE_COSTS.size[sizeLevel] || 0;
    document.getElementById('wc-size-stars').textContent = levelStars(sizeLevel, 3);
    document.getElementById('wc-size-cost').textContent = sizeMaxed ? 'MAX' : '$' + sizeCost;
    const sizeBtn = document.getElementById('wc-btn-size');
    if (sizeBtn) sizeBtn.disabled = sizeMaxed || money < sizeCost;

    // Also update per-player money cards
    for (const p of (state.players || [])) {
      const cards = document.querySelectorAll('.wc-money-value');
      // Find the card for this player by index
      const playerCards = document.getElementById('wc-players').children;
      if (playerCards[p.id]) {
        const moneyEl = playerCards[p.id].querySelector('.wc-money-value');
        if (moneyEl) moneyEl.textContent = '$' + (p.money || 0);
      }
    }
  }

  function hideWaveClear() {
    waveClearEl.classList.remove('active');
    document.getElementById('wc-shop').innerHTML = '';
  }

  function updateHUD(state) {
    document.getElementById('val-score').textContent = state.score;
    document.getElementById('val-wave').textContent = state.wave;
    const remaining = state.enemies.filter(e => e.alive).length + state.enemiesLeft;
    document.getElementById('val-enemies').textContent = remaining;

    // Per-player lives & money
    const myId = Network.getMyPlayerId();
    const me = state.players ? state.players.find(p => p.id === myId) : null;
    document.getElementById('val-lives').textContent = me ? me.lives : 0;
    document.getElementById('val-money').textContent = '$' + (me ? (me.money || 0) : 0);

    // Players count
    const playersEl = document.getElementById('val-players');
    if (playersEl && state.players) {
      playersEl.textContent = state.players.length;
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
    showWaveClear, hideWaveClear, updateWaveClear,
    updateHUD, setConnectionStatus, isMenuVisible
  };
})();
