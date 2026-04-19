const LaserRenderer = (() => {
  function draw(ctx, lasers) {
    if (!lasers || !lasers.length) return;
    for (const L of lasers) {
      const ttl = L.ttl || 0;
      if (ttl <= 0) continue;
      const a = Math.min(1, ttl / 15);
      const x0 = L.x0, y0 = L.y0, x1 = L.x1, y1 = L.y1;

      ctx.save();
      ctx.globalAlpha = 0.35 * a;
      ctx.strokeStyle = '#00e5ff';
      ctx.lineWidth = 10;
      ctx.lineCap = 'round';
      ctx.shadowColor = '#00e5ff';
      ctx.shadowBlur = 24 * a;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.stroke();

      ctx.globalAlpha = 0.85 * a;
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 3;
      ctx.shadowBlur = 12 * a;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.stroke();
      ctx.restore();
    }
  }

  return { draw };
})();
