(function () {
  const TEMPLATES = {
    topbar: `
<header class="navbar-floating">
    <div class="logo-section">
        <a href="https://tuxblox.net/">
            <img src="https://assetdelivery.tuxblox.net/images/png/banner/tuxblox-banner-offset-small.png" alt="TuxBlox Logo" class="brand-banner-img">
        </a>
    </div>
    <nav class="navbar-links">
        <a href="https://tuxblox.net/" class="nav-btn" data-route="/">About</a>
        <a href="https://tuxblox.net/install" class="nav-btn" data-route="/install">Setup</a>
        <a href="https://tuxblox.net/deployment" class="nav-btn" data-route="/deployment">Deployment</a>
        <a href="https://tuxblox.net/docs" class="nav-btn" data-route="/docs">Docs</a>
        <a href="https://tuxblox.net/discord" target="_blank" class="nav-btn">Discord</a>
        <a href="https://tuxblox.net/github" target="_blank" class="nav-btn nav-git">Source Code</a>
    </nav>
</header>
`,
    footer: `
<section class="disclaimer-box">
    <h4>Disclaimer</h4>
    <p>TuxBlox is an independent compatibility tool intended for personal use. It is not affiliated or associated with Roblox Corporation in any way.</p>
</section>
 
<footer>
    <div>&copy; 2026 TuxBlox Project (MIT License)</div>
    <div>GitHub: <a href="https://github.com/cherrypath0" target="_blank">@cherrypath0</a></div>
</footer>
`
  };
 
  function loadIncludes() {
    document.querySelectorAll('[data-include]').forEach((el) => {
      const key = el.getAttribute('data-include');
      const html = TEMPLATES[key];
 
      if (!html) {
        console.warn(`[include.js] No template found for "${key}"`);
        return;
      }
 
      el.innerHTML = html;
    });

    const currentPath = window.location.pathname;
    
    document.querySelectorAll('.navbar-links .nav-btn[data-route]').forEach((btn) => {
      const route = btn.getAttribute('data-route');
      
      if (currentPath === route || currentPath === route + '.html') {
        btn.classList.add('active');
      }
    });
 
    document.dispatchEvent(new CustomEvent('includes:loaded'));
  }
 
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', loadIncludes);
  } else {
    loadIncludes();
  }
})();