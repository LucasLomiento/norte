// Gera o site do Norte (GitHub Pages): junta o modelo de site/src com os
// textos de cada idioma (site/src/textos) e as releases de
// LucasLomiento/norte-releases (novidades, versão e links de download) e
// escreve em site/dist: português na raiz, inglês em en/ e espanhol em es/.
//
//   node site/build.mjs            (GITHUB_TOKEN opcional; sem ele, 60 pedidos/h)
//
// O Actions roda isto a cada push que mexe no site e depois de cada versão
// publicada (tools/publicar.sh dispara o .github/workflows/site.yml), então a
// página acompanha as versões sozinha.
//
// As notas das releases são escritas em português. A tradução de cada uma
// fica em site/notas/<idioma>/<tag>.md (en, es); sem ela, a página do idioma
// mostra a nota original, avisando que está em português.

import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const REPO = 'LucasLomiento/norte-releases'
const SITE = 'https://lucaslomiento.github.io/norte/'
const RAIZ = dirname(fileURLToPath(import.meta.url))
const SAIDA = join(RAIZ, 'dist')
/** Quantas versões aparecem abertas; o resto fica em "versões anteriores". */
const ABERTAS = 3
/** Os idiomas e a pasta de cada um no site (o primeiro é o original, na raiz). */
const IDIOMAS = [
  { id: 'pt', pasta: '' },
  { id: 'en', pasta: 'en/' },
  { id: 'es', pasta: 'es/' },
]

/** Endereço relativo de uma página (pasta do idioma) a partir de outra. */
const caminho = (raiz, pasta) => raiz + pasta || './'

const esc = (texto) =>
  texto.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;')

/** Negrito, código e link (o que as notas das releases usam). */
function inline(texto) {
  return esc(texto)
    .replace(/`([^`]+)`/g, '<code>$1</code>')
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/\[([^\]]+)\]\((https?:[^)\s]+)\)/g, '<a href="$2">$1</a>')
}

/** Notas de uma release em HTML: parágrafos e listas, sem o "Instalar ou atualizar" (igual em todas). */
export function notas(corpo) {
  const html = []
  let lista = false
  let paragrafo = []
  const fecharParagrafo = () => {
    if (paragrafo.length) html.push(`<p>${inline(paragrafo.join(' '))}</p>`)
    paragrafo = []
  }
  const fecharLista = () => {
    if (lista) html.push('</ul>')
    lista = false
  }
  for (const linha of corpo.replace(/\r/g, '').split('\n')) {
    const item = linha.match(/^\s*[-*]\s+(.*)$/)
    if (/^\*\*Instalar ou atualizar/.test(linha.trim())) {
      fecharParagrafo()
      fecharLista()
      continue
    }
    if (item) {
      fecharParagrafo()
      if (!lista) html.push('<ul>')
      lista = true
      html.push(`<li>${inline(item[1])}</li>`)
    } else if (!linha.trim()) {
      fecharParagrafo()
      fecharLista()
    } else {
      fecharLista()
      paragrafo.push(linha.trim())
    }
  }
  fecharParagrafo()
  fecharLista()
  return html.join('\n')
}

/** Datas no idioma da página, no fuso de onde o Norte é feito. */
function datas(intl) {
  const fuso = 'America/Sao_Paulo'
  return {
    dia: (iso) =>
      new Intl.DateTimeFormat(intl, { day: 'numeric', month: 'long', year: 'numeric', timeZone: fuso }).format(new Date(iso)),
    diaCurto: (iso) =>
      new Intl.DateTimeFormat(intl, { day: 'numeric', month: 'short', timeZone: fuso }).format(new Date(iso)).replace('.', ''),
    hora: (iso) => {
      const texto = new Intl.DateTimeFormat(intl, { hour: '2-digit', minute: '2-digit', timeZone: fuso }).format(new Date(iso))
      // Em português, "08h40".
      return intl.startsWith('pt') ? texto.replace(':', 'h') : texto
    },
  }
}
const megas = (bytes) => `${(bytes / 1024 / 1024).toFixed(0)} MB`

async function releases() {
  const cabecalhos = { Accept: 'application/vnd.github+json', 'User-Agent': 'norte-site' }
  if (process.env.GITHUB_TOKEN) cabecalhos.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`
  const resposta = await fetch(`https://api.github.com/repos/${REPO}/releases?per_page=100`, { headers: cabecalhos })
  if (!resposta.ok) throw new Error(`GitHub respondeu ${resposta.status} ao listar as releases`)
  const todas = await resposta.json()
  return todas
    .filter((r) => !r.draft && !r.prerelease)
    .sort((a, b) => new Date(b.published_at) - new Date(a.published_at))
}

/** "v155.0.8059.79-16" -> { chromium: "155.0.8059.79", pacote: "16" } */
function versao(tag) {
  const m = tag.replace(/^v/, '').match(/^(.*)-(\d+)$/)
  return m ? { chromium: m[1], pacote: m[2] } : { chromium: tag.replace(/^v/, ''), pacote: '' }
}

/** A nota da release no idioma (site/notas/<id>/<tag>.md), ou null. */
function traducao(id, tag) {
  if (id === 'pt') return null
  const arquivo = join(RAIZ, 'notas', id, `${tag}.md`)
  return existsSync(arquivo) ? readFileSync(arquivo, 'utf8') : null
}

function novidades(lista, id, t, d) {
  const blocos = lista.map((r, i) => {
    const v = versao(r.tag_name)
    const primeira = i === lista.length - 1
    const traduzida = traducao(id, r.tag_name)
    const original = id !== 'pt' && traduzida === null
    const titulo = i === 0 ? t.novidades.atual : primeira ? t.novidades.primeira : t.novidades.versao.replace('{n}', esc(v.pacote))
    return `<article class="versao${i === 0 ? ' atual' : ''}" id="${esc(r.tag_name)}">
  <div class="versao-marca" aria-hidden="true"><span></span></div>
  <header>
    <a class="versao-num" href="#${esc(r.tag_name)}"><span class="versao-pkg">${esc(v.pacote)}</span></a>
    <div>
      <h3>${titulo}</h3>
      <p class="versao-data"><time datetime="${esc(r.published_at)}">${esc(d.dia(r.published_at))}, ${esc(d.hora(r.published_at))}</time> · <code>${esc(r.tag_name)}</code></p>
    </div>
  </header>
  <div class="versao-notas"${original ? ' lang="pt-BR"' : ''}>${original ? `<p class="versao-original">${t.novidades.original}</p>\n` : ''}${notas(traduzida ?? r.body ?? '')}</div>
</article>`
  })
  const abertas = blocos.slice(0, ABERTAS).join('\n')
  const resto = blocos.slice(ABERTAS)
  if (!resto.length) return abertas
  return `${abertas}
<details class="anteriores">
  <summary>${t.novidades.anteriores} <span>${resto.length}</span></summary>
  ${resto.join('\n')}
</details>`
}

const perguntas = (t) =>
  t.perguntas.lista
    .map(
      ([pergunta, resposta]) => `    <details>
      <summary>${pergunta}</summary>
      <p>${resposta}</p>
    </details>`,
    )
    .join('\n')

/** O seletor de idioma do topo (funciona sem JavaScript: é um <details>). */
function seletor(atual, textos, raiz) {
  const t = textos[atual.id]
  const itens = IDIOMAS.map(({ id, pasta }) => {
    const i = textos[id].idioma
    const corrente = id === atual.id ? ' aria-current="page"' : ''
    return `          <li><a href="${caminho(raiz, pasta)}" hreflang="${i.lang}" lang="${i.lang}" data-idioma="${id}"${corrente}><span>${i.curto}</span>${i.nome}</a></li>`
  }).join('\n')
  return `      <details class="idiomas">
        <summary aria-label="${t.topo.idioma}: ${t.idioma.nome}"><svg class="i"><use href="#i-globe"/></svg><span>${t.idioma.curto}</span></summary>
        <ul>
${itens}
        </ul>
      </details>`
}

/** Troca {{chave.com.pontos}} pelos textos do idioma; chave que falta é erro. */
function traduzir(html, t, id) {
  return html.replace(/\{\{([a-z][\w]*(?:\.[\w]+)+)\}\}/g, (_, caminho) => {
    const valor = caminho.split('.').reduce((o, parte) => (o == null ? o : o[parte]), t)
    if (typeof valor !== 'string') throw new Error(`{{${caminho}}} sem texto em ${id}.mjs`)
    return valor
  })
}

const lista = await releases()
if (!lista.length) throw new Error('Nenhuma release publicada')
const atual = lista[0]
const pacote = atual.assets.find((a) => /\.pkg\.tar\.zst$/.test(a.name))
if (!pacote) throw new Error(`A release ${atual.tag_name} não tem o .pkg.tar.zst`)
const v = versao(atual.tag_name)

const textos = {}
for (const { id } of IDIOMAS) textos[id] = (await import(join(RAIZ, 'src/textos', `${id}.mjs`))).default

const modelo = readFileSync(join(RAIZ, 'src/index.html'), 'utf8')
const icones = readFileSync(join(RAIZ, 'src/icones.svg'), 'utf8')

rmSync(SAIDA, { recursive: true, force: true })
mkdirSync(SAIDA, { recursive: true })

for (const idioma of IDIOMAS) {
  const t = textos[idioma.id]
  const d = datas(t.idioma.intl)
  const raiz = idioma.pasta ? '../' : ''
  const sugestoes = {}
  for (const outro of IDIOMAS) {
    if (outro.id === idioma.id) continue
    sugestoes[outro.id] = { ...textos[outro.id].sugestao, lang: textos[outro.id].idioma.lang, url: caminho(raiz, outro.pasta) }
  }
  const js = { ...t.js, idioma: idioma.id, sugestoes }

  const valores = {
    VERSAO: esc(atual.tag_name.replace(/^v/, '')),
    CHROMIUM: esc(v.chromium),
    CHROMIUM_MAIOR: esc(v.chromium.split('.')[0]),
    PACOTE: esc(v.pacote),
    DATA: esc(d.dia(atual.published_at)),
    DATA_CURTA: esc(d.diaCurto(atual.published_at)),
    ARQUIVO: esc(pacote.name),
    TAMANHO: esc(megas(pacote.size)),
    LINK_PACOTE: esc(pacote.browser_download_url),
    LINK_VERSAO: esc(atual.html_url),
    NUM_VERSOES: String(lista.length),
    NOVIDADES: novidades(lista, idioma.id, t, d),
    IDIOMAS: seletor(idioma, textos, raiz),
    ALTERNATIVOS: [
      ...IDIOMAS.map(({ id, pasta }) => `<link rel="alternate" hreflang="${textos[id].idioma.lang}" href="${SITE}${pasta}">`),
      `<link rel="alternate" hreflang="x-default" href="${SITE}en/">`,
    ].join('\n'),
    OG_ALTERNATIVOS: IDIOMAS.filter(({ id }) => id !== idioma.id)
      .map(({ id }) => `<meta property="og:locale:alternate" content="${textos[id].idioma.og}">`)
      .join('\n'),
    ENDERECO: `${SITE}${idioma.pasta}`,
    SITE,
    RAIZ: raiz,
    // Dentro de <script>: sem "<" cru, nada fecha a tag antes da hora.
    TEXTOS_JS: JSON.stringify(js).replace(/</g, '\\u003c'),
    ICONES: icones,
    ANO: String(new Date().getFullYear()),
  }

  // Primeiro os textos (que podem ter chaves de dados dentro), depois os dados.
  let html = traduzir(modelo, t, idioma.id).replace('{{PERGUNTAS}}', () => perguntas(t))
  html = html.replace(/\{\{([A-Z_]+)\}\}/g, (_, chave) => {
    if (!(chave in valores)) throw new Error(`{{${chave}}} sem valor`)
    return valores[chave]
  })
  const sobra = html.match(/\{\{[^}]*\}\}/)
  if (sobra) throw new Error(`${sobra[0]} sobrou na página ${idioma.id}`)

  mkdirSync(join(SAIDA, idioma.pasta), { recursive: true })
  writeFileSync(join(SAIDA, idioma.pasta, 'index.html'), html)
}

for (const arquivo of ['style.css', 'main.js']) cpSync(join(RAIZ, 'src', arquivo), join(SAIDA, arquivo))
cpSync(join(RAIZ, 'assets'), join(SAIDA, 'assets'), { recursive: true })
writeFileSync(join(SAIDA, '.nojekyll'), '')
const semTraducao = IDIOMAS.filter(({ id }) => id !== 'pt').flatMap(({ id }) =>
  lista.filter((r) => traducao(id, r.tag_name) === null).map((r) => `${id}/${r.tag_name}`),
)
console.log(`site/dist: ${atual.tag_name}, ${lista.length} versões, ${pacote.name} (${megas(pacote.size)}), idiomas ${IDIOMAS.map((i) => i.id).join(' ')}`)
if (semTraducao.length) console.log(`Notas sem tradução (aparecem em português): ${semTraducao.join(', ')}`)
