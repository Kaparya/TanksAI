const WallRenderer = (() => {
  const TILE = GameConstants.TILE;
  const COLS = GameConstants.COLS, ROWS = GameConstants.ROWS;

  function draw(ctx, walls, theme) {
    for (let y = 0; y < ROWS; y++) for (let x = 0; x < COLS; x++) {
      const px = x * TILE, py = y * TILE;
      if (walls[y][x] === 1) {
        // Brick
        ctx.fillStyle = theme.brick1;
        ctx.fillRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.fillStyle = theme.brick2;
        ctx.fillRect(px + 2, py + 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + TILE / 2, py + 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + 2, py + TILE / 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.fillRect(px + TILE / 2, py + TILE / 2, TILE / 2 - 3, TILE / 2 - 3);
        ctx.strokeStyle = theme.brickLine;
        ctx.lineWidth = 1;
        ctx.strokeRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.beginPath();
        ctx.moveTo(px + 1, py + TILE / 2);
        ctx.lineTo(px + TILE - 1, py + TILE / 2);
        ctx.moveTo(px + TILE / 2, py + 1);
        ctx.lineTo(px + TILE / 2, py + TILE - 1);
        ctx.stroke();
        ctx.fillStyle = theme.brickHi;
        ctx.fillRect(px + 2, py + 2, TILE - 4, 2);
      } else if (walls[y][x] === 2) {
        // Steel
        ctx.fillStyle = theme.steel1;
        ctx.fillRect(px + 1, py + 1, TILE - 2, TILE - 2);
        ctx.fillStyle = theme.steel2;
        ctx.fillRect(px + 3, py + 3, TILE - 6, TILE - 6);
        ctx.fillStyle = theme.steel3;
        ctx.fillRect(px + 6, py + 6, TILE - 12, TILE - 12);
        ctx.fillStyle = theme.steelRivet;
        const cs = [[4,4],[TILE-6,4],[4,TILE-6],[TILE-6,TILE-6]];
        for (const [cx, cy] of cs) {
          ctx.beginPath(); ctx.arc(px + cx, py + cy, 2, 0, Math.PI * 2); ctx.fill();
        }
        ctx.fillStyle = theme.steelHi;
        ctx.fillRect(px + 2, py + 2, TILE - 4, 2);
      }
    }
  }

  return { draw };
})();
