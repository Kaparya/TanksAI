const TankRenderer = (() => {
  function draw(ctx, tank, isPlayer, frameCount) {
    if (!tank || !tank.alive) return;
    const { x, y, dir, color } = tank;
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(dir * Math.PI / 2);

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,.35)';
    ctx.fillRect(-11, -9, 24, 24);

    // Body
    ctx.fillStyle = (tank.flash > 0) ? '#ffffff' : color;
    ctx.fillRect(-12, -12, 24, 24);

    // Body gradient
    const grad = ctx.createLinearGradient(-12, -12, 12, 12);
    grad.addColorStop(0, 'rgba(255,255,255,.15)');
    grad.addColorStop(1, 'rgba(0,0,0,.2)');
    ctx.fillStyle = grad;
    ctx.fillRect(-12, -12, 24, 24);

    // Tracks
    const trackColor = 'rgba(0,180,210,.55)';
    ctx.fillStyle = trackColor;
    ctx.fillRect(-14, -14, 6, 28);
    ctx.fillRect(8, -14, 6, 28);

    // Track detail (animated)
    ctx.strokeStyle = isPlayer ? 'rgba(0,229,255,.35)' : 'rgba(255,130,80,.35)';
    ctx.lineWidth = 1;
    const trackOffset = (frameCount * (isPlayer ? 2 : 1)) % 4;
    for (let i = -14 + trackOffset; i < 14; i += 4) {
      ctx.beginPath(); ctx.moveTo(-14, i); ctx.lineTo(-8, i); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(8, i); ctx.lineTo(14, i); ctx.stroke();
    }

    // Turret
    ctx.fillStyle = '#1a1a2e';
    ctx.beginPath(); ctx.arc(0, 0, 7, 0, Math.PI * 2); ctx.fill();

    // Barrel
    const barrelGrad = ctx.createLinearGradient(-2.5, -18, 2.5, -4);
    barrelGrad.addColorStop(0, '#666');
    barrelGrad.addColorStop(1, '#333');
    ctx.fillStyle = barrelGrad;
    ctx.fillRect(-2.5, -18, 5, 14);
    ctx.fillStyle = '#888';
    ctx.fillRect(-3.5, -19, 7, 3);

    ctx.restore();

    // HP bar for multi-hp enemies
    if (!isPlayer && tank.maxHp > 1) {
      const bw = 24;
      const hpRatio = tank.hp / tank.maxHp;
      ctx.fillStyle = 'rgba(0,0,0,.6)';
      ctx.fillRect(x - bw / 2 - 1, y - 22, bw + 2, 5);
      ctx.fillStyle = hpRatio > .5 ? '#00e676' : hpRatio > .25 ? '#ff9100' : '#ff5252';
      ctx.fillRect(x - bw / 2, y - 21, bw * hpRatio, 3);
    }

    // Invulnerability shield
    if (isPlayer && tank.invuln > 0 && Math.floor(tank.invuln / 4) % 2 === 0) {
      ctx.save();
      ctx.globalAlpha = .25;
      ctx.strokeStyle = '#00e5ff';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(x, y, 18, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
    }
  }

  return { draw };
})();
