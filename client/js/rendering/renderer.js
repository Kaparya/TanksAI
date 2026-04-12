const Renderer = (() => {
  const canvas = document.getElementById('c');
  const ctx = canvas.getContext('2d');

  function draw(state) {
    if (!state || !state.walls) return;
    const theme = Themes.getTheme(state);

    ctx.save();

    // Screen shake
    if (state.screenShake > 0) {
      const sx = (Math.random() - .5) * state.screenShake * 1.5;
      const sy = (Math.random() - .5) * state.screenShake * 1.5;
      ctx.translate(sx, sy);
    }

    // Floor
    ctx.fillStyle = theme.bg;
    ctx.fillRect(0, 0, GameConstants.W, GameConstants.H);

    // Grid
    ctx.strokeStyle = theme.grid;
    ctx.lineWidth = 1;
    for (let x = 0; x <= GameConstants.W; x += GameConstants.TILE) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, GameConstants.H); ctx.stroke();
    }
    for (let y = 0; y <= GameConstants.H; y += GameConstants.TILE) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(GameConstants.W, y); ctx.stroke();
    }

    WallRenderer.draw(ctx, state.walls, theme);
    if (state.players) {
      for (const p of state.players) TankRenderer.draw(ctx, p, true, state.frameCount);
    }
    for (const e of state.enemies) TankRenderer.draw(ctx, e, false, state.frameCount);
    BulletRenderer.draw(ctx, state.bullets);
    ParticleRenderer.draw(ctx, state.particles);
    EffectsRenderer.drawWaveAnnouncement(ctx, state);

    ctx.restore();
    MinimapRenderer.draw(state, theme);
  }

  function clear() {
    ctx.fillStyle = '#181c24';
    ctx.fillRect(0, 0, GameConstants.W, GameConstants.H);
  }

  return { draw, clear };
})();
