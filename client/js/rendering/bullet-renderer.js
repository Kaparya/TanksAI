const BulletRenderer = (() => {
  function draw(ctx, bullets) {
    if (!bullets) return;
    for (const b of bullets) {
      const isEnemy = b.owner < 0;
      const bulletColor = isEnemy ? '#ff5252' : (b.explosive ? '#ff6d00' : '#ffd740');
      const glowColor = isEnemy ? 'rgba(255,82,82,.35)' : (b.explosive ? 'rgba(255,109,0,.45)' : 'rgba(255,215,64,.35)');
      const r = b.radius || 3.5;
      const glowR = r * 2.5;

      // Bullet body
      ctx.fillStyle = bulletColor;
      ctx.beginPath(); ctx.arc(b.x, b.y, r, 0, Math.PI * 2); ctx.fill();

      // Glow
      const glowGrad = ctx.createRadialGradient(b.x, b.y, 0, b.x, b.y, glowR);
      glowGrad.addColorStop(0, glowColor);
      glowGrad.addColorStop(1, 'transparent');
      ctx.fillStyle = glowGrad;
      ctx.beginPath(); ctx.arc(b.x, b.y, glowR, 0, Math.PI * 2); ctx.fill();

      // Simple trail
      ctx.globalAlpha = .2;
      ctx.fillStyle = bulletColor;
      ctx.beginPath(); ctx.arc(b.x - b.vx, b.y - b.vy, r * 0.7, 0, Math.PI * 2); ctx.fill();
      ctx.beginPath(); ctx.arc(b.x - b.vx * 2, b.y - b.vy * 2, r * 0.4, 0, Math.PI * 2); ctx.fill();
      ctx.globalAlpha = 1;
    }
  }

  return { draw };
})();
