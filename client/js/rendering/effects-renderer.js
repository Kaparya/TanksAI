const EffectsRenderer = (() => {
  const W = GameConstants.W, H = GameConstants.H;

  function drawWaveAnnouncement(ctx, state) {
    if (state.gameState !== 'playing') return;
    if (state.enemies.length === 0 && state.enemiesLeft > 0) {
      ctx.save();
      ctx.textAlign = 'center'; ctx.textBaseline = 'middle';

      ctx.fillStyle = 'rgba(10,12,16,.75)';
      ctx.fillRect(W / 2 - 140, H / 2 - 40, 280, 80);
      ctx.strokeStyle = 'rgba(0,229,255,.3)';
      ctx.lineWidth = 1;
      ctx.strokeRect(W / 2 - 140, H / 2 - 40, 280, 80);

      ctx.font = 'bold 32px Orbitron, monospace';
      ctx.fillStyle = '#00e5ff';
      ctx.shadowColor = '#00e5ff'; ctx.shadowBlur = 20;
      ctx.fillText('WAVE ' + state.wave, W / 2, H / 2 - 8);
      ctx.shadowBlur = 0;

      ctx.font = '14px "Share Tech Mono", monospace';
      ctx.fillStyle = '#576574';
      ctx.fillText(state.enemiesLeft + ' enemies incoming', W / 2, H / 2 + 22);
      ctx.restore();
    }
  }

  return { drawWaveAnnouncement };
})();
