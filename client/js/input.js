// ═══════════════════════════════════════════════════
//  Keyboard input capture
// ═══════════════════════════════════════════════════

const Input = (() => {
  const keys = {
    up: false, down: false, left: false, right: false, shoot: false
  };

  const keyMap = {
    'ArrowUp': 'up', 'KeyW': 'up',
    'ArrowDown': 'down', 'KeyS': 'down',
    'ArrowLeft': 'left', 'KeyA': 'left',
    'ArrowRight': 'right', 'KeyD': 'right',
    'Space': 'shoot'
  };

  const preventDefaults = new Set(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Space']);

  window.addEventListener('keydown', (e) => {
    if (preventDefaults.has(e.code)) e.preventDefault();

    const action = keyMap[e.code];
    if (action) keys[action] = true;

    // Escape for pause toggle
    if (e.code === 'Escape') {
      if (typeof App !== 'undefined') App.togglePause();
    }
  });

  window.addEventListener('keyup', (e) => {
    const action = keyMap[e.code];
    if (action) keys[action] = false;
  });

  function getState() {
    return { ...keys };
  }

  function reset() {
    keys.up = keys.down = keys.left = keys.right = keys.shoot = false;
  }

  return { getState, reset };
})();
