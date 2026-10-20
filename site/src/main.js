// Site do Norte: a bússola do começo (a agulha segue o ponteiro), a bússola
// do canto (aponta pra seção em que você está), a janela de demonstração, as
// fotos, os botões de copiar, o aparecer ao rolar e o idioma.

const $ = (seletor, raiz = document) => raiz.querySelector(seletor)
const $$ = (seletor, raiz = document) => [...raiz.querySelectorAll(seletor)]
const semMovimento = matchMedia('(prefers-reduced-motion: reduce)').matches
/** Textos no idioma da página (site/src/textos, postos pelo build.mjs). */
const TEXTOS = JSON.parse($('#textos').textContent)

/** Menor diferença entre dois ângulos, em graus (-180 a 180). */
const diferenca = (de, para) => ((((para - de) % 360) + 540) % 360) - 180

// ───────────── Topo ─────────────

const topo = $('.topo')
const aoRolar = () => topo.classList.toggle('rolou', scrollY > 8)
addEventListener('scroll', aoRolar, { passive: true })
aoRolar()

// ───────────── A bússola grande ─────────────

{
  const bussola = $('.bussola-grande')
  const agulha = $('.bg-agulha')
  const graus = $('.bg-graus')
  let atual = 0
  let alvo = 0
  let ultimoMovimento = -Infinity
  let rodando = false

  const escrever = (angulo) => {
    agulha.style.setProperty('--angulo', `${angulo}deg`)
    const g = Math.round(((angulo % 360) + 360) % 360) % 360
    graus.textContent = `${String(g).padStart(3, '0')}°`
  }

  if (semMovimento) {
    escrever(0)
  } else {
    addEventListener(
      'pointermove',
      (e) => {
        if (e.pointerType === 'touch') return
        const r = bussola.getBoundingClientRect()
        alvo = (Math.atan2(e.clientY - (r.top + r.height / 2), e.clientX - (r.left + r.width / 2)) * 180) / Math.PI + 90
        ultimoMovimento = performance.now()
      },
      { passive: true },
    )

    const quadro = (agora) => {
      if (!rodando) return
      // Parado, volta pro norte e oscila de leve, como uma bússola de verdade.
      const destino =
        agora - ultimoMovimento > 2200 ? Math.sin(agora / 1300) * 5 + Math.sin(agora / 3100) * 3 : alvo
      atual += diferenca(atual, destino) * 0.075
      escrever(atual)
      requestAnimationFrame(quadro)
    }

    new IntersectionObserver(([entrada]) => {
      const via = entrada.isIntersecting
      if (via && !rodando) {
        rodando = true
        requestAnimationFrame(quadro)
      } else if (!via) {
        rodando = false
      }
    }).observe(bussola)
  }
}

// ───────────── A bússola do canto e o menu ─────────────

{
  const guia = $('.bussola-guia')
  const agulha = $('.bgu-agulha')
  const secoes = $$('.secao[data-rumo]')
  const letras = $$('.bussola-guia a')
  const rotas = $$('.rotas a')
  let rumo = 0

  const apontar = (secao) => {
    // Gira pelo caminho mais curto, sem dar a volta inteira de 270° pra 0°.
    rumo += diferenca(rumo, Number(secao.dataset.rumo))
    agulha.style.setProperty('--rumo', `${rumo}deg`)
    const alvo = `#${secao.id}`
    for (const a of [...letras, ...rotas]) a.classList.toggle('ativa', a.getAttribute('href') === alvo)
  }

  const visiveis = new Map()
  const observador = new IntersectionObserver(
    (entradas) => {
      for (const e of entradas) visiveis.set(e.target, e.isIntersecting)
      const atual = secoes.find((s) => visiveis.get(s))
      if (atual) apontar(atual)
    },
    { rootMargin: '-45% 0px -50% 0px' },
  )
  secoes.forEach((s) => observador.observe(s))

  // Aparece depois do começo da página.
  new IntersectionObserver(([e]) => guia.classList.toggle('visivel', !e.isIntersecting), {
    rootMargin: '-120px 0px 0px 0px',
  }).observe($('.abertura'))
}

// ───────────── A janela de demonstração ─────────────

{
  // As cores ficam aqui; os nomes, títulos e domínios vêm no idioma da página.
  const CORES = [
    { cor: '#4f8cff', fixos: ['#ef4444', '#22c55e', '#e4e4e7'], abas: ['#f59e0b', '#38bdf8', '#22c55e', '#f472b6'] },
    { cor: '#f59e0b', container: true, fixos: ['#ef4444', '#4f8cff', '#22c55e'], abas: ['#ef4444', '#22c55e', '#4f8cff', '#a1a1aa'] },
    { cor: '#a78bfa', fixos: ['#a78bfa', '#f59e0b', '#38bdf8'], abas: ['#f43f5e', '#a78bfa', '#f59e0b', '#38bdf8'] },
  ]
  const ESPACOS = TEXTOS.espacos.map((espaco, i) => ({
    ...CORES[i],
    nome: espaco.nome,
    abas: espaco.abas.map(([titulo, dominio], j) => [titulo, dominio, CORES[i].abas[j]]),
  }))
  const ativas = ESPACOS.map(() => 0)

  const janela = $('#janela')
  const conteudo = $('.j-conteudo', janela)
  const fixos = $('.j-fixos', janela)
  const abas = $('.j-abas', janela)
  const nome = $('.j-espaco', janela)
  const bolinhas = $('.j-bolinhas', janela)
  const endereco = $('.j-endereco', janela)
  const botaoEsconder = $('#esconder')
  const dica = $('#dica')
  let atual = 0
  let trocando = false

  const el = (tag, classe) => {
    const e = document.createElement(tag)
    if (classe) e.className = classe
    return e
  }

  ESPACOS.forEach((espaco, i) => {
    const b = el('button', 'j-bolinha')
    b.type = 'button'
    b.setAttribute('role', 'tab')
    b.setAttribute('aria-label', espaco.nome)
    b.style.setProperty('--c', espaco.cor)
    b.addEventListener('click', () => {
      pararDemo()
      trocar(i)
    })
    bolinhas.append(b)
  })

  const mostrarAba = () => {
    const espaco = ESPACOS[atual]
    const [titulo, dominio, cor] = espaco.abas[ativas[atual]]
    janela.style.setProperty('--site', cor)
    endereco.innerHTML = ''
    const b = el('b')
    b.textContent = dominio
    endereco.append(b, ` / ${titulo}`)
    $$('.j-aba', abas).forEach((li, i) => li.classList.toggle('ativa', i === ativas[atual]))
  }

  const desenhar = () => {
    const espaco = ESPACOS[atual]
    janela.style.setProperty('--cor', espaco.cor)
    nome.textContent = espaco.nome
    fixos.replaceChildren(
      ...espaco.fixos.map((c) => {
        const f = el('div', 'j-fixo')
        const i = el('i')
        i.style.setProperty('--c', c)
        f.append(i)
        return f
      }),
    )
    abas.replaceChildren(
      ...espaco.abas.map(([titulo, , cor], i) => {
        const li = el('li', `j-aba${espaco.container ? ' container' : ''}`)
        const icone = el('i')
        icone.style.setProperty('--c', cor)
        const texto = el('span')
        texto.textContent = titulo
        li.append(icone, texto)
        li.addEventListener('click', () => {
          pararDemo()
          ativas[atual] = i
          mostrarAba()
        })
        return li
      }),
    )
    $$('.j-bolinha', bolinhas).forEach((b, i) => b.setAttribute('aria-selected', String(i === atual)))
    mostrarAba()
  }

  const trocar = (proximo) => {
    proximo = (proximo + ESPACOS.length) % ESPACOS.length
    if (proximo === atual || trocando) return
    const praFrente = proximo > atual || (atual === ESPACOS.length - 1 && proximo === 0)
    if (semMovimento) {
      atual = proximo
      desenhar()
      return
    }
    trocando = true
    conteudo.classList.remove('entrando', 'pela-esquerda')
    conteudo.classList.add('saindo')
    conteudo.classList.toggle('pra-direita', !praFrente)
    setTimeout(() => {
      atual = proximo
      desenhar()
      conteudo.classList.remove('saindo', 'pra-direita')
      conteudo.classList.add('entrando')
      conteudo.classList.toggle('pela-esquerda', !praFrente)
      trocando = false
    }, 170)
  }

  // Sozinha, a janela vai passando pelos espaços até alguém mexer nela.
  let relogio = null
  let mexeu = false
  const pararDemo = () => {
    mexeu = true
    clearInterval(relogio)
    relogio = null
  }
  if (!semMovimento) {
    new IntersectionObserver(([e]) => {
      if (e.isIntersecting && !mexeu && !relogio) {
        relogio = setInterval(() => trocar(atual + 1), 3200)
      } else if (!e.isIntersecting && relogio) {
        clearInterval(relogio)
        relogio = null
      }
    }, { threshold: 0.5 }).observe(janela)
  }

  janela.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
      e.preventDefault()
      pararDemo()
      trocar(atual + (e.key === 'ArrowRight' ? 1 : -1))
    } else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
      // O Ctrl+S do Norte, aqui só com o foco na janela de mentira.
      e.preventDefault()
      alternarLateral()
    }
  })

  // Deslizar de lado em cima da lateral também troca, como no touchpad.
  let inicioX = null
  $('.j-lateral', janela).addEventListener('pointerdown', (e) => {
    inicioX = e.clientX
  })
  addEventListener('pointerup', (e) => {
    if (inicioX === null) return
    const dx = e.clientX - inicioX
    inicioX = null
    if (Math.abs(dx) > 50) {
      pararDemo()
      trocar(atual + (dx < 0 ? 1 : -1))
    }
  })

  // Lateral escondida: a borda da janela traz ela de volta por cima da
  // página, e ela some 150 ms depois que o ponteiro sai (as folgas do Norte).
  let folga = null
  const cancelarFolga = () => {
    clearTimeout(folga)
    folga = null
  }
  const revelar = () => {
    cancelarFolga()
    janela.classList.add('revelada')
  }
  const esconderDepois = () => {
    if (folga) return
    folga = setTimeout(() => {
      folga = null
      janela.classList.remove('revelada')
    }, 150)
  }
  const lateral = $('.j-lateral', janela)
  $('.j-borda', janela).addEventListener('pointerenter', revelar)
  $('.j-borda', janela).addEventListener('click', revelar)
  // Aberta por cima da página: fica enquanto o ponteiro estiver em cima
  // dela (ou na borda), e conta a folga quando ele sai, por onde for.
  addEventListener(
    'pointermove',
    (e) => {
      if (!janela.classList.contains('revelada')) return
      const r = lateral.getBoundingClientRect()
      const j = janela.getBoundingClientRect()
      const dentro =
        e.clientX >= j.left && e.clientX <= Math.max(r.right, j.left + 14) && e.clientY >= j.top && e.clientY <= j.bottom
      if (dentro) cancelarFolga()
      else esconderDepois()
    },
    { passive: true },
  )

  const DICA_NORMAL = dica.innerHTML
  const alternarLateral = () => {
    pararDemo()
    const escondida = !janela.classList.contains('escondida')
    janela.classList.toggle('escondida', escondida)
    janela.classList.remove('revelada')
    botaoEsconder.setAttribute('aria-pressed', String(escondida))
    $('span', botaoEsconder).textContent = escondida ? TEXTOS.mostrar : TEXTOS.esconder
    dica.innerHTML = escondida ? TEXTOS.dica_escondida : DICA_NORMAL
  }
  botaoEsconder.addEventListener('click', alternarLateral)

  desenhar()
}

// ───────────── Fotos ─────────────

{
  const abas = $$('.fotos-abas [role=tab]')
  const escolher = (aba) => {
    for (const a of abas) {
      const sim = a === aba
      a.setAttribute('aria-selected', String(sim))
      a.tabIndex = sim ? 0 : -1
      $(`#${a.getAttribute('aria-controls')}`).hidden = !sim
    }
  }
  abas.forEach((aba, i) => {
    aba.addEventListener('click', () => escolher(aba))
    aba.addEventListener('keydown', (e) => {
      if (e.key !== 'ArrowRight' && e.key !== 'ArrowLeft') return
      const outra = abas[(i + (e.key === 'ArrowRight' ? 1 : abas.length - 1)) % abas.length]
      escolher(outra)
      outra.focus()
    })
  })
}

// ───────────── Copiar os comandos ─────────────

$$('.copiar').forEach((botao) => {
  const icone = $('use', botao)
  botao.addEventListener('click', async () => {
    const texto = $(`#${botao.dataset.copiar}`).innerText.replace(/^\$ /gm, '')
    try {
      await navigator.clipboard.writeText(texto)
      botao.classList.add('copiado')
      icone.setAttribute('href', '#i-check')
      botao.setAttribute('aria-label', TEXTOS.copiado)
      setTimeout(() => {
        botao.classList.remove('copiado')
        icone.setAttribute('href', '#i-copy')
        botao.setAttribute('aria-label', TEXTOS.copiar)
      }, 1600)
    } catch {
      // Sem permissão pra área de transferência: o texto continua selecionável.
    }
  })
})

// ───────────── Aparecer ao rolar ─────────────

{
  const alvos = $$(
    '.secao-cabeca, .demo, .recurso, .fotos, .teclado, .medida, .bloqueio, .cartao-grande, .pilha, .link-cartao, .passos li, .versao, .perguntas',
  )
  if (semMovimento || !('IntersectionObserver' in window)) {
    // Fica tudo à vista.
  } else {
    alvos.forEach((a) => a.classList.add('revelar'))
    const observador = new IntersectionObserver(
      (entradas) => {
        for (const e of entradas) {
          if (!e.isIntersecting) continue
          e.target.classList.add('visto')
          observador.unobserve(e.target)
        }
      },
      { rootMargin: '0px 0px -8% 0px', threshold: 0.08 },
    )
    alvos.forEach((a) => observador.observe(a))
  }

  // As barras das medidas crescem quando aparecem.
  const medidas = $('.medidas')
  if (medidas) {
    new IntersectionObserver(
      ([e], obs) => {
        if (!e.isIntersecting) return
        medidas.classList.add('vista')
        obs.disconnect()
      },
      { threshold: 0.3 },
    ).observe(medidas)
  }
}

// ───────────── Idioma ─────────────

{
  const CHAVE = 'norte-idioma'
  const lembrar = (id) => {
    try {
      localStorage.setItem(CHAVE, id)
    } catch {
      // Sem armazenamento (janela anônima, bloqueado): o aviso volta na próxima visita.
    }
  }
  const escolhido = () => {
    try {
      return localStorage.getItem(CHAVE)
    } catch {
      return null
    }
  }

  // O seletor do topo: fecha clicando fora ou com Esc; escolher um idioma
  // fica lembrado (e o aviso abaixo não aparece mais).
  const seletor = $('.idiomas')
  if (seletor) {
    addEventListener('click', (e) => {
      if (seletor.open && !seletor.contains(e.target)) seletor.open = false
    })
    seletor.addEventListener('keydown', (e) => {
      if (e.key !== 'Escape' || !seletor.open) return
      seletor.open = false
      $('summary', seletor).focus()
    })
    $$('a[data-idioma]', seletor).forEach((a) => a.addEventListener('click', () => lembrar(a.dataset.idioma)))
  }

  // Navegador noutro idioma que o site também tem: um aviso discreto, no
  // idioma da pessoa, com o link pra versão dela. Nada de redirecionar sozinho.
  if (!escolhido()) {
    const preferido = (navigator.languages || [navigator.language || ''])
      .map((l) => l.slice(0, 2).toLowerCase())
      .find((l) => l === TEXTOS.idioma || l in TEXTOS.sugestoes)
    const s = preferido && TEXTOS.sugestoes[preferido]
    if (s) {
      const aviso = document.createElement('div')
      aviso.className = 'sugestao-idioma'
      aviso.lang = s.lang
      aviso.setAttribute('role', 'region')
      aviso.setAttribute('aria-label', s.texto)
      const texto = document.createElement('p')
      texto.textContent = s.texto
      const link = document.createElement('a')
      link.href = s.url
      link.hreflang = s.lang
      link.textContent = s.link
      link.addEventListener('click', () => lembrar(preferido))
      const fechar = document.createElement('button')
      fechar.type = 'button'
      fechar.className = 'icone-botao'
      fechar.setAttribute('aria-label', s.fechar)
      fechar.innerHTML = '<svg class="i"><use href="#i-x"/></svg>'
      fechar.addEventListener('click', () => {
        lembrar(TEXTOS.idioma)
        aviso.remove()
      })
      aviso.append(texto, link, fechar)
      document.body.append(aviso)
    }
  }
}
