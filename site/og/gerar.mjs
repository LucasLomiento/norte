// Gera as imagens de compartilhamento do site (1200x630), uma por idioma:
// assets/og.png (português), og-en.png e og-es.png. O texto vem de
// site/src/textos (chave "og") e as letras da bússola, de "rumos".
//
//   node site/og/gerar.mjs        (CHROME=/caminho/do/chrome; padrão: chromium)
//
// Não roda no Actions: as imagens ficam no repositório. Rodar de novo só
// quando o texto ou o desenho mudar.

import { execFileSync } from 'node:child_process'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const RAIZ = dirname(dirname(fileURLToPath(import.meta.url)))
const CHROME = process.env.CHROME || 'chromium'
const fontes = pathToFileURL(join(RAIZ, 'assets/fontes')).href

// Riscos do mostrador: um a cada 6°, mais longo e forte a cada 30°.
const riscos = Array.from({ length: 60 }, (_, i) => {
  const forte = i % 5 === 0
  return `<line x1="0" y1="${forte ? -228 : -224}" x2="0" y2="-212" transform="rotate(${i * 6})"
    stroke="rgba(255,255,255,${forte ? 0.42 : 0.16})" stroke-width="${forte ? 3 : 1.5}" stroke-linecap="round"/>`
}).join('')

function pagina(t) {
  return `<!doctype html>
<html lang="${t.idioma.lang}"><head><meta charset="utf-8">
<style>
@font-face { font-family: Bricolage; src: url(${fontes}/bricolage-grotesque-latin-opsz-normal.woff2); font-weight: 200 800; }
@font-face { font-family: Bricolage; src: url(${fontes}/bricolage-grotesque-latin-ext-opsz-normal.woff2); font-weight: 200 800;
  unicode-range: U+0100-02BA, U+1E00-1EFF; }
@font-face { font-family: Geist; src: url(${fontes}/geist-latin-wght-normal.woff2); font-weight: 100 900; }
@font-face { font-family: Geist Mono; src: url(${fontes}/geist-mono-latin-wght-normal.woff2); font-weight: 100 900; }
* { box-sizing: border-box; margin: 0; }
html, body { background: #09090b; height: 630px; overflow: hidden; width: 1200px; }
body { color: #f4f4f5; font-family: Geist, sans-serif; position: relative; }
.marca { align-items: center; display: flex; font: 650 28px/1 Bricolage; gap: 18px; left: 78px; letter-spacing: -.02em; position: absolute; top: 66px; }
.marca svg { height: 30px; width: 30px; }
h1 { font: 700 92px/1 Bricolage; left: 72px; letter-spacing: -.045em; position: absolute; top: 186px; white-space: nowrap; }
p.texto { color: #a1a1aa; font-size: 23px; left: 74px; line-height: 1.5; position: absolute; top: 400px; width: 520px; }
p.url { color: #71717a; font: 400 17px Geist Mono; left: 74px; letter-spacing: .02em; position: absolute; top: 553px; }
.bussola { height: 520px; left: 630px; position: absolute; top: 55px; width: 520px; }
</style></head><body>
<div class="marca"><svg viewBox="0 0 256 256"><g transform="rotate(45 128 128)">
  <path d="M128 4 L80 128 L128 128 Z" fill="#8cb6ff"/><path d="M128 4 L176 128 L128 128 Z" fill="#2b6cf5"/>
  <path d="M80 128 L128 252 L128 128 Z" fill="#d4d4d8"/><path d="M176 128 L128 252 L128 128 Z" fill="#8b8b94"/></g>
  <circle cx="128" cy="128" r="14" fill="#fff"/><circle cx="128" cy="128" r="6.5" fill="#2b6cf5"/></svg>Norte</div>
<h1>${t.og.titulo}</h1>
<p class="texto">${t.og.texto}</p>
<p class="url">lucaslomiento.github.io/norte</p>
<svg class="bussola" viewBox="-260 -260 520 520">
  <defs>
    <radialGradient id="brilho"><stop offset="0" stop-color="#2b6cf5" stop-opacity=".34"/><stop offset=".55" stop-color="#2b6cf5" stop-opacity=".08"/><stop offset="1" stop-color="#2b6cf5" stop-opacity="0"/></radialGradient>
    <pattern id="pontos" width="22" height="22" patternUnits="userSpaceOnUse"><circle cx="11" cy="11" r="1" fill="rgba(255,255,255,.05)"/></pattern>
  </defs>
  <circle r="235" fill="url(#pontos)"/>
  <circle r="240" fill="url(#brilho)"/>
  <circle r="236" fill="none" stroke="rgba(255,255,255,.16)" stroke-width="1.5"/>
  ${riscos}
  <g font-family="Geist Mono" font-size="16" text-anchor="middle" dominant-baseline="central" fill="#a1a1aa">
    <text y="-178" fill="#8cb6ff">${t.rumos.n}</text><text x="178">${t.rumos.l}</text>
    <text y="178">${t.rumos.s}</text><text x="-178">${t.rumos.o}</text>
  </g>
  <g transform="rotate(27) translate(-138 -138) scale(1.078)">
    <path d="M128 10 L86 128 L128 128 Z" fill="#8cb6ff"/><path d="M128 10 L170 128 L128 128 Z" fill="#2b6cf5"/>
    <path d="M86 128 L128 246 L128 128 Z" fill="#d4d4d8"/><path d="M170 128 L128 246 L128 128 Z" fill="#8b8b94"/>
    <circle cx="128" cy="128" r="13" fill="#fff"/><circle cx="128" cy="128" r="6" fill="#2b6cf5"/>
  </g>
</svg>
<script>
  // Título comprido (outros idiomas): diminui até caber ao lado da bússola.
  document.fonts.ready.then(() => {
    const h1 = document.querySelector('h1')
    let tamanho = 92
    while (h1.scrollWidth > 530 && tamanho > 60) h1.style.fontSize = (tamanho -= 2) + 'px'
  })
</script>
</body></html>`
}

const temp = mkdtempSync(join(tmpdir(), 'norte-og-'))
try {
  for (const [id, saida] of [['pt', 'og.png'], ['en', 'og-en.png'], ['es', 'og-es.png']]) {
    const t = (await import(join(RAIZ, 'src/textos', `${id}.mjs`))).default
    const html = join(temp, `${id}.html`)
    writeFileSync(html, pagina(t))
    execFileSync(CHROME, [
      '--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1',
      `--user-data-dir=${join(temp, 'perfil')}`, '--window-size=1200,630', '--virtual-time-budget=3000',
      `--screenshot=${join(RAIZ, 'assets', saida)}`, pathToFileURL(html).href,
    ], { stdio: 'ignore' })
    console.log(`assets/${saida}`)
  }
} finally {
  rmSync(temp, { recursive: true, force: true })
}
