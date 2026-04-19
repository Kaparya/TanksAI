const Shop = (() => {
  const waveClearEl = document.getElementById('wave-clear');

  const UPGRADE_COSTS = {
    damage: [0, 25, 75],
    size:   [0, 20, 60],
    armor:  [0, 250, 1000],
    explosive: 300, // one-time, mirrors EXPLOSIVE_AMMO_COST in server/features/economy/shop.cpp
  };

  function levelStars(level, max) {
    return '\u2605'.repeat(level) + '\u2606'.repeat(max - level);
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
          '<div class="wc-item-icon">\uD83D\uDCA5</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Bullet Damage</div>' +
            '<div class="wc-item-stars" id="wc-dmg-stars">\u2605\u2606\u2606</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-dmg-cost">$20</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-damage">Buy</button>' +
        '</div>' +
        '<div class="wc-shop-item">' +
          '<div class="wc-item-icon">\u2B55</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Bullet Size</div>' +
            '<div class="wc-item-stars" id="wc-size-stars">\u2605\u2606\u2606</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-size-cost">$15</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-size">Buy</button>' +
        '</div>' +
        '<div class="wc-shop-item">' +
          '<div class="wc-item-icon">\uD83D\uDEE1\uFE0F</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Armor</div>' +
            '<div class="wc-item-desc">Increases your Health</div>' +
            '<div class="wc-item-stars" id="wc-armor-stars">\u2605\u2606\u2606</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-armor-cost">$25</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-armor">Buy</button>' +
        '</div>' +
        '<div class="wc-shop-item wc-shop-item-wide">' +
          '<div class="wc-item-icon">\uD83D\uDCA3</div>' +
          '<div class="wc-item-info">' +
            '<div class="wc-item-name">Explosive Ammo</div>' +
            '<div class="wc-item-desc">AoE damage on impact</div>' +
            '<div class="wc-item-stars" id="wc-exp-stars">\u2014</div>' +
          '</div>' +
          '<div class="wc-item-cost" id="wc-exp-cost">$300</div>' +
          '<button class="btn wc-buy-btn" id="wc-btn-explosive">Buy</button>' +
        '</div>' +
      '</div>';

    document.getElementById('wc-btn-damage').addEventListener('click', () => Network.sendBuy('damage'));
    document.getElementById('wc-btn-size').addEventListener('click', () => Network.sendBuy('size'));
    document.getElementById('wc-btn-armor').addEventListener('click', () => Network.sendBuy('armor'));
    document.getElementById('wc-btn-explosive').addEventListener('click', () => Network.sendBuy('explosive'));

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

    // Armor upgrade
    const armorLevel = me.armorLevel || 1;
    const armorMaxed = armorLevel >= 3;
    const armorCost = UPGRADE_COSTS.armor[armorLevel] || 0;
    const armorStarsEl = document.getElementById('wc-armor-stars');
    const armorCostEl = document.getElementById('wc-armor-cost');
    const armorBtn = document.getElementById('wc-btn-armor');
    if (armorStarsEl) armorStarsEl.textContent = levelStars(armorLevel, 3);
    if (armorCostEl) armorCostEl.textContent = armorMaxed ? 'MAX' : '$' + armorCost;
    if (armorBtn) armorBtn.disabled = armorMaxed || money < armorCost;

    // Explosive ammo (one-time)
    const hasExplosive = !!me.explosiveAmmo;
    const expCost = UPGRADE_COSTS.explosive;
    const expStarsEl = document.getElementById('wc-exp-stars');
    const expCostEl = document.getElementById('wc-exp-cost');
    const expBtn = document.getElementById('wc-btn-explosive');
    if (expStarsEl) expStarsEl.textContent = hasExplosive ? 'Owned' : 'Not owned';
    if (expCostEl) expCostEl.textContent = hasExplosive ? '\u2014' : '$' + expCost;
    if (expBtn) expBtn.disabled = hasExplosive || money < expCost;

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

  return { showWaveClear, updateWaveClear, hideWaveClear };
})();
