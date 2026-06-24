(function () {
  const AUTH_URL = 'https://auth.tuxblox.net';

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
        <div id="nav-auth-pill"></div>
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

  function esc(s) {
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
  }

  function getAuth() {
    try { return JSON.parse(localStorage.getItem('tuxblox_auth') || 'null'); }
    catch { return null; }
  }

  function renderPill(auth) {
    const pillEl = document.getElementById('nav-auth-pill');
    if (!pillEl) return;

    if (!auth) {
      pillEl.innerHTML = '<a href="/login" class="nav-btn nav-login-pill">Login</a>';
      return;
    }

    const avatar = auth.avatarUrl
      ? `<img src="${esc(auth.avatarUrl)}" class="nav-user-avatar" onerror="this.style.display='none'" alt="">`
      : '';
    const display = esc(auth.robloxDisplayName || auth.username || '');
    const uname   = esc(auth.robloxUsername   || auth.username || '');

    pillEl.innerHTML = `
      <div class="nav-user-pill" id="nav-user-pill-wrap">
        ${avatar}
        <div class="nav-user-info">
          <span class="nav-user-display">${display}</span>
          <span class="nav-user-name">@${uname}</span>
        </div>
        <button class="nav-logout-btn" id="nav-logout-btn" title="Log out">&#x2715;</button>
      </div>`;

    document.getElementById('nav-logout-btn').addEventListener('click', async () => {
      try {
        await fetch(`${AUTH_URL}/api/logout`, { method: 'POST', credentials: 'include' });
      } catch {}
      localStorage.removeItem('tuxblox_auth');
      window.location.href = '/';
    });
  }

  function loadIncludes() {
    document.querySelectorAll('[data-include]').forEach((el) => {
      const key = el.getAttribute('data-include');
      const html = TEMPLATES[key];
      if (!html) { console.warn(`[include.js] No template found for "${key}"`); return; }
      el.innerHTML = html;
    });

    const currentPath = window.location.pathname;
    document.querySelectorAll('.navbar-links .nav-btn[data-route]').forEach((btn) => {
      const route = btn.getAttribute('data-route');
      if (currentPath === route || currentPath === route + '.html') btn.classList.add('active');
    });

    // On the main domain, render from localStorage immediately (no flash).
    // On subdomains (docs), skip this — their localStorage is isolated, so we
    // wait for the verify call below to avoid briefly flashing a stale login link.
    const MAIN_HOSTS = ['tuxblox.net', 'www.tuxblox.net'];
    if (MAIN_HOSTS.includes(window.location.hostname)) {
      renderPill(getAuth());
    }

    // Always verify via the session cookie — this handles cross-subdomain auth
    // (docs.tuxblox.net has its own localStorage so we can't rely on it there)
    fetch(`${AUTH_URL}/api/verify`, { credentials: 'include' })
      .then(async r => {
        if (r.ok) {
          const data = await r.json();
          if (data.authenticated) {
            const authData = {
              username: data.username,
              robloxId: data.robloxId,
              robloxUsername: data.robloxUsername,
              robloxDisplayName: data.robloxDisplayName,
              avatarUrl: data.avatarUrl,
            };
            try { localStorage.setItem('tuxblox_auth', JSON.stringify(authData)); } catch {}
            renderPill(authData);
          } else {
            try { localStorage.removeItem('tuxblox_auth'); } catch {}
            renderPill(null);
          }
        } else {
          try { localStorage.removeItem('tuxblox_auth'); } catch {}
          renderPill(null);
        }
      })
      .catch(() => {});

    document.dispatchEvent(new CustomEvent('includes:loaded'));
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', loadIncludes);
  } else {
    loadIncludes();
  }
})();
