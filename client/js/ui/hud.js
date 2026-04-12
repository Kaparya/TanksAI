const HUD = (() => {
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

  return { updateHUD };
})();
