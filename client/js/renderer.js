// ═══════════════════════════════════════════════════
//  Canvas renderer — draws game state from server
// ═══════════════════════════════════════════════════

const Renderer = (() => {
  const canvas = document.getElementById('c');
  const ctx = canvas.getContext('2d');
  const miniCanvas = document.getElementById('minimap');
  const mctx = miniCanvas.getContext('2d');

  const W = 800, H = 600;
  const TILE = 40;
  const COLS = 20, ROWS = 15;

  // Bullet trails (client-side visual only)
  const bulletTrails = new Map(); // keyed by "x,y" approx
  let trailId = 0;

  function draw(state) {
    if (!state || !state.walls) return;

    ctx.save();

    // Screen shake
    if (state.screenShake > 0) {
      const sx = (Math.random() - .5) * state.screenShake * 1.5;
      const sy = (Math.random() - .5) * state.screenShake * 1.5;
      ctx.translate(sx, sy);
    }

    // Floor
    ctx.fillStyle = '#181c24';
    ctx.fillRect(0, 0, W, H);

    // Grid
    ctx.strokeStyle = 'rgba(255,255,255,.03)';
    ctx.lineWidth = 1;
    for (let x = 0; x <= W; x += TILE) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke();
    }
    for (let y = 0; y <= H; y += TILE) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
    }

    drawWalls(state.walls);
    if (state.players) {
      for (const p of state.players) drawTank(p, true, state.frameCount);
    }
    for (const e of state.enemies) drawTank(e, false, state.frameCount);
    drawBullets(state.bullets);
    drawParticles(state.particles);
    drawWaveAnnouncement(state);

    ctx.restore();
    drawMinimap(state);
  }

  function drawWalls(walls) {
    for (let y = 0; y < ROWS; y++) for (let x = 0; x < COLS; x++) {
      const px = x * TILE, py = y * TILE;
      if (walls[y][x] === 1) {
        // Brick
        ctx.fillStyle = '#7a3a2a';
        ctx.fillRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.fillStyle = '#8b4533';
        ctx.fillRect(px + 2, py + 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + TILE / 2, py + 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + 2, py + TILE / 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + TILE / 2, py + TILE / 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.strokeStyle = '#5a2a1a';
        ctx.lineWidth = 1;
        ctx.strokeRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.beginPath();
        ctx.moveTo(px + 1, py + TILE / 2);
        ctx.lineTo(px + TILE - 1, py + TILE / 2);
        ctx.moveTo(px + TILE / 2, py + 1);
        ctx.lineTo(px + TILE / 2, py + TILE - 1);
        ctx.stroke();
        ctx.fillStyle = 'rgba(255,255,255,.06)';
        ctx.fillRect(px + 2, py + 2, TILE - 4, 2);
      } else if (walls[y][x] === 2) {
        // Steel
        ctx.fillStyle = '#4a4e5c';
        ctx.fillRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.fillStyle = '#5c6070';
        ctx.fillRect(px + 3, py + 3, TILE - 6, TILE - 6);
        ctx.fillStyle = '#4a4e5c';
        ctx.fillRect(px + 6, py + 6, TILE - 12, TILE - 12);
        ctx.fillStyle = '#6a6e7c';
        const cs = [[4,4],[TILE-6,4],[4,TILE-6],[TILE-6,TILE-6]];
        for (const [cx, cy] of cs) {
          ctx.beginPath(); ctx.arc(px + cx, py + cy, 2, 0, Math.PI * 2); ctx.fill();
        }
        ctx.fillStyle = 'rgba(255,255,255,.08)';
        ctx.fillRect(px + 2, py + 2, TILE - 4, 2);
      }
    }
  }

  function drawTank(tank, isPlayer, frameCount) {
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

  function drawBullets(bullets) {
    if (!bullets) return;
    for (const b of bullets) {
      const isEnemy = b.owner < 0;
      const bulletColor = isEnemy ? '#ff5252' : '#ffd740';
      const glowColor = isEnemy ? 'rgba(255,82,82,.35)' : 'rgba(255,215,64,.35)';
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

  function drawParticles(particles) {
    if (!particles) return;
    for (const p of particles) {
      ctx.globalAlpha = Math.max(0, p.life / p.maxLife);
      ctx.fillStyle = p.color;
      const r = p.size * (p.life / p.maxLife);
      ctx.beginPath(); ctx.arc(p.x, p.y, r, 0, Math.PI * 2); ctx.fill();
    }
    ctx.globalAlpha = 1;
  }

  function drawWaveAnnouncement(state) {
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

  function drawMinimap(state) {
    if (!state || !state.walls) return;
    mctx.fillStyle = '#0a0c10';
    mctx.fillRect(0, 0, 120, 90);

    const sx = 120 / W, sy = 90 / H;

    for (let y = 0; y < ROWS; y++) for (let x = 0; x < COLS; x++) {
      if (state.walls[y][x] === 1) {
        mctx.fillStyle = '#553322';
        mctx.fillRect(x * TILE * sx, y * TILE * sy, TILE * sx, TILE * sy);
      } else if (state.walls[y][x] === 2) {
        mctx.fillStyle = '#556';
        mctx.fillRect(x * TILE * sx, y * TILE * sy, TILE * sx, TILE * sy);
      }
    }

    for (const e of state.enemies) {
      if (!e.alive) continue;
      mctx.fillStyle = e.color;
      mctx.fillRect(e.x * sx - 2, e.y * sy - 2, 4, 4);
    }

    if (state.players) {
      for (const p of state.players) {
        if (!p.alive) continue;
        mctx.fillStyle = p.color;
        mctx.fillRect(p.x * sx - 2, p.y * sy - 2, 4, 4);
      }
    }
  }

  function clear() {
    ctx.fillStyle = '#181c24';
    ctx.fillRect(0, 0, W, H);
  }

  return { draw, clear };
})();
