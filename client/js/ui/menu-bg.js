const MenuBg = (() => {
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

  return { drawMenuBg };
})();
