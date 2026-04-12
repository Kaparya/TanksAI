const Themes = (() => {
  const THEMES = {
    standard: {
      bg: '#181c24', grid: 'rgba(255,255,255,.03)',
      brick1: '#7a3a2a', brick2: '#8b4533', brickLine: '#5a2a1a',
      brickHi: 'rgba(255,255,255,.06)',
      steel1: '#4a4e5c', steel2: '#5c6070', steel3: '#4a4e5c',
      steelRivet: '#6a6e7c', steelHi: 'rgba(255,255,255,.08)',
      mmBg: '#0a0c10', mmBrick: '#553322', mmSteel: '#556',
    },
    snow: {
      bg: '#d0dce8', grid: 'rgba(0,0,40,.04)',
      brick1: '#a8c8e0', brick2: '#c0ddf0', brickLine: '#7aa0be',
      brickHi: 'rgba(255,255,255,.15)',
      steel1: '#8090a0', steel2: '#98aab8', steel3: '#8090a0',
      steelRivet: '#b0c0cc', steelHi: 'rgba(255,255,255,.18)',
      mmBg: '#b8c8d8', mmBrick: '#8ab0cc', mmSteel: '#6888a0',
    },
    sand: {
      bg: '#2e2818', grid: 'rgba(255,220,160,.04)',
      brick1: '#b08850', brick2: '#c49860', brickLine: '#8a6830',
      brickHi: 'rgba(255,240,200,.08)',
      steel1: '#8a7a60', steel2: '#a0906e', steel3: '#8a7a60',
      steelRivet: '#b0a080', steelHi: 'rgba(255,240,200,.10)',
      mmBg: '#1e1808', mmBrick: '#907040', mmSteel: '#706050',
    },
    city: {
      bg: '#101218', grid: 'rgba(0,200,255,.03)',
      brick1: '#585c64', brick2: '#686e78', brickLine: '#40444c',
      brickHi: 'rgba(0,220,255,.06)',
      steel1: '#282c34', steel2: '#363a44', steel3: '#282c34',
      steelRivet: '#00ccff', steelHi: 'rgba(0,220,255,.10)',
      mmBg: '#08090c', mmBrick: '#44484e', mmSteel: '#223',
    },
  };

  function getTheme(state) {
    return THEMES[state.mapTheme] || THEMES.standard;
  }

  return { getTheme, THEMES };
})();
