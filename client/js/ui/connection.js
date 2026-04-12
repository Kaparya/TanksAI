const ConnectionUI = (() => {
  const connDot = document.getElementById('conn-dot');
  const connText = document.getElementById('conn-text');

  function setConnectionStatus(status) {
    connDot.className = '';
    if (status === 'connected') {
      connDot.classList.add('connected');
      connText.textContent = 'Connected';
    } else if (status === 'connecting') {
      connDot.classList.add('connecting');
      connText.textContent = 'Connecting...';
    } else {
      connText.textContent = 'Disconnected';
    }
  }

  return { setConnectionStatus };
})();
