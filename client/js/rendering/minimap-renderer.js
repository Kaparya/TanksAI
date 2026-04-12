const MinimapRenderer = (() => {
  const mctx = document.getElementById('minimap').getContext('2d');
  const TILE = GameConstants.TILE;
  const COLS = GameConstants.COLS, ROWS = GameConstants.ROWS;
  const W = GameConstants.W, H = GameConstants.H;

  function draw(state, theme) {
    if (!state || !state.walls) return;
    mctx.fillStyle = theme.mmBg;
    mctx.fillRect(0, 0, 120, 90);

    const sx = 120 / W, sy = 90 / H;

    for (let y = 0; y < ROWS; y++) for (let x = 0; x < COLS; x++) {
      if (state.walls[y][x] === 1) {
        mctx.fillStyle = theme.mmBrick;
        mctx.fillRect(x * TILE * sx, y * TILE * sy, TILE * sx, TILE * sy);
      } else if (state.walls[y][x] === 2) {
        mctx.fillStyle = theme.mmSteel;
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

  return { draw };
})();
