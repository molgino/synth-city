// Service Worker der Handy-App: alles aus dem Cache (offline), Updates im Hintergrund (naechster Start)
const CACHE = 'synthcity-32f9e51a2e81', FILES = ["./", "index.html", "local.js", "sync.js", "synthcity.js", "synthcity.wasm", "manifest.json", "VERSION", "icon-180.png", "icon-192.png", "icon-512.png"];
self.addEventListener('install', e => {
  // am HTTP-Cache vorbei laden (GitHub Pages haelt Dateien 10 min: sonst landen alte Dateien in der neuen Version)
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES.map(u => new Request(u, { cache: 'reload' })))).then(() => self.skipWaiting()));
});
self.addEventListener('activate', e => {
  e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k.startsWith('synthcity-') && k !== CACHE).map(k => caches.delete(k))))
    .then(() => self.clients.claim()));
});
self.addEventListener('fetch', e => {
  const u = new URL(e.request.url);
  if (e.request.method !== 'GET' || u.origin !== location.origin) return;   // Sync zum Geraet geht nicht ueber den Cache
  e.respondWith(caches.match(e.request, {ignoreSearch: true}).then(r => r || fetch(e.request)));
});
