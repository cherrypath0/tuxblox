(function () {
  const TEMPLATES = {
    topbar: `
<header>
    <div class="logo-section">
        <a href="/">
            <img src="/tuxblox-banner-offset-small.png" alt="TuxBlox Logo" class="brand-banner-img">
        </a>
    </div>
    <nav>
        <a href="/">About</a>
        <a href="/install">Setup</a>
        <a href="/docs">Docs</a>
        <a href="/discord" target="_blank">Discord</a>
        <a href="/github" target="_blank" class="nav-git">Source Code</a>
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
 
    document.dispatchEvent(new CustomEvent('includes:loaded'));
  }
 
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', loadIncludes);
  } else {
    loadIncludes();
  }
})();