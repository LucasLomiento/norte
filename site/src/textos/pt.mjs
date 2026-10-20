// Textos do site em português (o idioma original). As chaves são as mesmas
// em en.mjs e es.mjs; o build.mjs troca cada {{chave.com.pontos}} do
// src/index.html pelo texto daqui. Valores podem ter HTML e as chaves de
// dados em maiúsculas ({{PACOTE}}, {{DATA}}...), que entram depois.

export default {
  idioma: {
    lang: 'pt-BR',
    og: 'pt_BR',
    intl: 'pt-BR',
    nome: 'Português',
    curto: 'PT',
    og_imagem: 'og.png',
  },

  head: {
    titulo: 'Norte · navegue sem se perder',
    descricao:
      'Navegador para Linux feito em cima do Chromium: abas numa lateral que some, um espaço para cada parte da vida, contas separadas em containers e bloqueador de anúncios de série. Sem IA e sem conta Google.',
    og_descricao: 'Chromium com abas numa lateral que some, espaços, containers e bloqueador de série. Sem IA, sem conta Google.',
  },

  // A imagem de compartilhamento (assets/og*.png, feita pelo site/og/gerar.mjs).
  og: {
    titulo: 'Navegue sem<br>se perder.',
    texto: 'Navegador para Linux, em cima do Chromium: lateral que some, espaços, containers e bloqueador de série. Sem IA.',
  },

  topo: {
    pular: 'Pular para o conteúdo',
    marca: 'Norte, início',
    secoes: 'Seções',
    github: 'Código no GitHub',
    baixar: 'Baixar',
    idioma: 'Idioma',
  },

  // Os pontos da bússola: a letra de cada seção e o nome dela.
  rumos: {
    n: 'N',
    l: 'L',
    s: 'S',
    o: 'O',
    navegar: 'Navegar',
    leve: 'Leve',
    sigilo: 'Sigilo',
    origem: 'Origem',
    rumo: 'rumo',
    guia: 'Seções da página',
  },

  abertura: {
    versao: 'Versão {{PACOTE}} · {{DATA_CURTA}}',
    titulo: 'Navegue sem<br>se perder.',
    lead: 'O Norte é um navegador para Linux feito em cima do Chromium. As abas moram numa lateral que some quando você não precisa dela, cada parte da sua vida ganha um espaço, e as contas de casa e do trabalho não se misturam.',
    baixar: 'Baixar para Arch Linux',
    testar: 'Testar aqui mesmo',
    meta: 'Chromium {{CHROMIUM_MAIOR}} · {{TAMANHO}} · grátis e de código aberto',
    resumo: 'Em resumo',
    fato_extensoes: 'Extensões da Chrome Web Store',
    fato_streaming: 'Netflix e Spotify',
    fato_bloqueador: 'Bloqueador de anúncios de série',
    fato_sem_ia: 'Sem IA e sem conta Google',
    fato_brasil: 'Feito no Brasil',
  },

  navegar: {
    titulo: 'A tela é da página.',
    lead: 'Tudo o que o Norte põe em volta do site foi pensado para sair do caminho: as abas, a barra de endereço e até a cor da janela.',
    demo: 'demonstração',
    demo_rotulo: 'Janela do Norte de demonstração. Use as setas para trocar de espaço.',
    demo_espaco: 'Pessoal',
    demo_nova: 'Nova guia',
    demo_espacos: 'Espaços',
    demo_endereco: '<b>receitas.exemplo.com</b> / Pão de fermentação natural',
    esconder: 'Esconder a lateral',
    dica: 'Clique nas bolinhas no pé da lateral, ou use <span class="teclas"><kbd>←</kbd> <kbd>→</kbd></span>, para trocar de espaço.',

    lateral_t: 'A lateral que some',
    lateral: 'As abas ficam na esquerda, em lista, com o título inteiro. <span class="teclas"><kbd>Ctrl</kbd> <kbd>S</kbd></span> esconde a lateral; encoste o ponteiro na borda e ela volta por cima da página, e some de novo quando você sai.',
    espacos_t: 'Um espaço pra cada coisa',
    espacos: 'Pessoal, Trabalho, Faculdade: cada espaço tem suas abas, seus sites fixados e sua cor, e a janela muda de cor junto. Troque pelos botões laterais do mouse, deslizando no touchpad ou com <span class="teclas"><kbd>Ctrl</kbd> <kbd>Alt</kbd> <kbd>←</kbd> <kbd>→</kbd></span>.',
    fixados_t: 'Fixados que não fecham',
    fixados: 'Os sites de todo dia ficam no alto do espaço, como ícones ou numa lista com pastas. Clique e a aba abre no lugar deles; feche e o site continua lá, esperando.',
    dividir_t: 'Duas abas lado a lado',
    dividir: '<span class="teclas"><kbd>Ctrl</kbd> <kbd>Alt</kbd> <kbd>V</kbd></span> divide a tela entre duas abas; <kbd>H</kbd> põe uma em cima da outra. Arrastar uma aba até a borda da página também divide.',
    barra_t: 'Barra de endereço sob demanda',
    barra: '<span class="teclas"><kbd>Ctrl</kbd> <kbd>Shift</kbd> <kbd>S</kbd></span> esconde a barra de endereço. Ela desce por cima da página quando o ponteiro chega no alto, ou quando você aperta <span class="teclas"><kbd>Ctrl</kbd> <kbd>L</kbd></span>.',
    cor_t: 'A cor do site na barra',
    cor: 'A barra de endereço pega a cor do topo da página, e a janela parece parte do site. Parada, ela mostra só o domínio e o título; o endereço inteiro aparece quando você chega perto.',
    nomes_t: 'Abas com o nome que você quiser',
    nomes: 'Clique duas vezes no nome de uma aba, de um site fixado ou de uma pasta e escreva outro. O nome fica salvo e volta quando você abre o Norte.',
    nova_aba_t: 'Uma nova aba calma',
    nova_aba: 'Hora, data e os seus atalhos. Sem notícias, sem sugestões, sem anúncio. O que aparece se escolhe no canto da própria página.',

    fotos_t: 'Sem maquete: fotos da versão de verdade.',
    fotos: 'Fotos',
    foto_pessoal: 'Espaço Pessoal',
    foto_trabalho: 'Espaço Trabalho',
    foto_nova_aba: 'Nova aba',
    foto_pessoal_alt: 'Janela do Norte no espaço Pessoal, com a Wikipédia aberta e as abas na lateral',
    foto_trabalho_alt: 'O espaço Trabalho, em laranja, com abas no container do trabalho',
    foto_nova_aba_alt: 'A nova aba do Norte, com a hora grande no meio',

    teclado_t: 'Tudo também pelo teclado',
    atalho_lateral: 'Mostrar ou esconder a lateral',
    atalho_barra: 'Mostrar ou esconder a barra de endereço',
    atalho_trocar: 'Espaço anterior e próximo',
    atalho_ir: 'Ir direto para um espaço',
    atalho_novo: 'Novo espaço',
    atalho_dividir: 'Dividir a tela lado a lado · em cima e embaixo',
    atalho_trocar_lados: 'Trocar as abas da divisão de lado',
    atalho_config: 'Configurações do Norte (onde dá pra trocar todos estes)',
  },

  leve: {
    titulo: 'Leve, e com números.',
    lead: 'Comparamos o Norte com o Zen na mesma máquina, com perfis novos. É uma máquina só, não um laboratório: os seus números vão ser outros, mas a diferença costuma ficar.',
    abrir: 'Para abrir a janela',
    abrir_valor: '<span>0,17</span> s',
    abrir_aria: 'Norte 0,17 segundo; Zen 0,66 segundo',
    abrir_norte: '0,17 s',
    abrir_zen: '0,66 s',
    abrir_nota: 'Quase quatro vezes mais rápido.',
    memoria: 'Memória com 6 sites abertos',
    memoria_valor: '<span>1,07</span> GB',
    memoria_aria: 'Norte 1,07 gigabyte; Zen 1,44 gigabyte',
    memoria_norte: '1,07 GB',
    memoria_zen: '1,44 GB',
    memoria_nota: 'Um quarto a menos.',
    velocidade: 'Velocidade das páginas (Speedometer 3.1)',
    velocidade_valor: '<span>31,0</span>',
    velocidade_aria: 'Norte 31,0; Zen 32,6; Chromium do Arch 29,5',
    velocidade_norte: '31,0',
    velocidade_zen: '32,6',
    velocidade_chromium: '29,5',
    velocidade_nota: 'Empate: os três rodam a internet no mesmo ritmo.',
    bloqueio_t: 'O bloqueador também é desempenho',
    bloqueio: 'Anúncio que não carrega não gasta processador. O Norte barra os anúncios e rastreadores antes da requisição sair, e os quadros de anúncio nem chegam a ser criados.',
    portais: 'Seis portais de notícia abertos',
    portais_valor: '<b>≈18</b> processos <s>≈80</s>',
    g1: 'O g1 parado, uma aba',
    g1_valor: '<b>4%</b> de CPU <s>35%</s>',
    uol: 'O UOL parado, uma aba',
    uol_valor: '<b>810 MB</b> <s>1,26 GB</s>',
    rodape: 'Medido em 15 de outubro de 2026 com perfis novos, renderização por software (Xvfb) e o Zen 1.23. Riscado: o mesmo teste sem o bloqueador.',
  },

  sigilo: {
    titulo: 'O que é seu, separado.',
    lead: 'Contas que não se misturam, anúncios que não seguem você e uma janela anônima que esquece de verdade.',
    containers_t: 'Containers: contas separadas na mesma janela',
    containers: 'Um container é uma caixa só dele de cookies, logins e cache. O e-mail pessoal e o do trabalho abertos ao mesmo tempo, cada um no seu, sem perfil novo e sem janela anônima.',
    containers_espaco: 'Cada espaço pode ter um container: as abas que você abre nele já nascem lá dentro, e um link aberto a partir de uma aba fica no container dela.',
    caixa_site: 'mail.exemplo.com',
    caixa_pessoal: 'Pessoal',
    caixa_trabalho: 'Trabalho',
    conta_pessoal: '<span>voce</span>@pessoal.com',
    conta_trabalho: '<span>voce</span>@empresa.com',
    cookies: 'Cookies',
    logins: 'Logins',
    cache: 'Cache',
    anonima_t: 'Uma janela anônima que esquece',
    anonima: '<span class="teclas"><kbd>Ctrl</kbd> <kbd>Shift</kbd> <kbd>N</kbd></span> abre uma janela limpa, sem espaços nem containers, que não enxerga os cookies e logins das suas outras janelas. Quando você fecha, o Norte apaga o histórico, os cookies e os logins dela.',
    anonima_alt: 'Janela anônima do Norte, com uma bússola de contorno e o título Janela anônima',
    bloqueador_t: 'Bloqueador de série',
    bloqueador: 'EasyList, EasyPrivacy e a lista brasileira já vêm ligadas e atualizam a cada versão. O Norte barra anúncios e rastreadores na rede e ainda esconde o espaço que eles deixariam vazio. Se um site quebrar, dá pra desligar só para ele, no cadeado da barra.',
    sem_ia_t: 'Sem IA, sem conta, sem estatísticas',
    sem_ia: 'Os recursos de IA do Chromium ficam desligados e fora dos menus. Não tem login do Google nem envio de dados de uso. A única conversa do Norte com a internet por conta própria é procurar versão nova no GitHub, e dá pra desligar.',
  },

  origem: {
    titulo: 'Feito no Brasil, com o código à vista.',
    lead: 'O Norte é um projeto pessoal, de código aberto. Por baixo está o Chromium de verdade, não um Electron: os sites funcionam como no Chrome, e as mudanças do Norte ficam em patches pequenos, para acompanhar as versões novas do Chromium.',
    pilha: 'Como o Norte é feito',
    camada_norte: 'lateral, espaços, containers, fixados, bloqueador, atualizador',
    patches_t: 'Patches',
    camada_patches: 'ganchos pequenos no código do Chromium, um por área',
    camada_chromium: 'o motor do Chrome, com Widevine para Netflix e Spotify',
    codigo_t: 'Código',
    codigo: 'Patches, código novo e scripts de compilação (BSD-3-Clause)',
    versoes_t: 'Todas as versões',
    versoes: '{{NUM_VERSOES}} versões publicadas, com pacote e notas',
    issues_t: 'Problemas e ideias',
    issues: 'Abra uma issue no GitHub',
  },

  instalar: {
    titulo: 'Instalar',
    sub: 'Versão {{VERSAO}}, de {{DATA}}. Para Arch Linux e derivados (CachyOS, EndeavourOS, Manjaro), x86_64.',
    repo: 'Acrescente o repositório do Norte no fim do <code>/etc/pacman.conf</code>',
    instale: 'Instale',
    pronto: 'Pronto. As atualizações vêm junto com as do sistema',
    pronto_nota: 'O próprio Norte também avisa no pé da lateral quando sai versão nova, com um botão "Atualizar".',
    copiar: 'Copiar',
    arquivo: 'Só o arquivo ({{TAMANHO}})',
    arquivo_nota: 'Depois: <code>sudo pacman -U {{ARQUIVO}}</code>. Os pacotes não são assinados (<code>SigLevel = Optional TrustAll</code>): o pacman confia no que vem das versões do GitHub. Windows e macOS ainda não.',
  },

  novidades: {
    titulo: 'Novidades',
    sub: 'O que mudou em cada versão, do jeito que saiu. Também <a href="{{LINK_VERSAO}}">no GitHub</a>.',
    atual: 'Versão atual',
    primeira: 'Primeira versão pública',
    versao: 'Versão {n}',
    anteriores: 'Versões anteriores',
    // Notas sem tradução (só nos outros idiomas).
    original: '',
  },

  perguntas: {
    titulo: 'Perguntas',
    lista: [
      ['É mesmo o Chromium por baixo?', 'É. O Norte é o Chromium {{CHROMIUM}} com as mudanças dele por cima, e os patches estão todos no repositório. Os sites funcionam como no Chrome, e as extensões da Chrome Web Store instalam e funcionam normalmente.'],
      ['Netflix e Spotify funcionam?', 'Funcionam. O Widevine, o componente que esses sites pedem, é baixado sozinho na primeira vez que você abre um deles. No Linux, os próprios serviços limitam a resolução em qualquer navegador.'],
      ['Dá pra trazer tudo do Zen?', 'Dá. Em Configurações do Norte → Geral → Importar do Zen, ele traz containers, espaços, sites fixados e pastas, os cookies de cada container (os logins), senhas, favoritos e histórico. Pode deixar o Zen aberto.'],
      ['E o 1Password?', 'Funciona: o pacote já instala a ligação entre a extensão e o aplicativo do 1Password e põe o Norte na lista de navegadores em que o app confia. É só instalar a extensão.'],
      ['Por que não tem IA?', 'Porque a ideia é um navegador que só navega. Os recursos de IA do Chromium (Gemini, "Ajude a escrever", busca com IA, organizar guias) ficam desligados e fora dos menus, e nada da sua navegação vai para um assistente.'],
      ['O que o Norte manda pra internet?', 'Por conta própria, só uma consulta ao GitHub para saber se tem versão nova (desliga em Configurações do Norte → Geral → Atualizações). Não tem conta do Google, sincronização nem estatísticas de uso.'],
      ['Ele se chamava Navegador?', 'Até a versão 20, sim. Quem já tinha instalado recebeu o Norte na atualização de sempre, com o perfil inteiro junto: logins, espaços, containers e extensões.'],
      ['Quem faz?', 'É um projeto pessoal do <a href="https://github.com/LucasLomiento">@LucasLomiento</a>, de código aberto (BSD-3-Clause). Encontrou um problema ou teve uma ideia? <a href="https://github.com/LucasLomiento/norte/issues">Abra uma issue</a>.'],
    ],
  },

  rodape: {
    slogan: 'Navegue sem se perder.',
    links: 'Links',
    codigo: 'Código',
    versoes: 'Versões',
    problemas: 'Problemas',
    instalar: 'Instalar',
    legal: 'O Norte não é um produto do Google. Chromium é marca do Google LLC; Netflix, Spotify, Zen e 1Password são marcas dos seus donos. Ícones do Lucide (ISC); fontes Geist e Bricolage Grotesque (OFL). © {{ANO}}, feito no Brasil.',
  },

  // Aviso pra quem tem o navegador noutro idioma: aparece na página dos
  // outros idiomas, escrito neste.
  sugestao: {
    texto: 'Esta página também existe em português.',
    link: 'Ler em português',
    fechar: 'Fechar',
  },

  // Textos que o main.js usa (vão pra página num <script type="application/json">).
  js: {
    espacos: [
      {
        nome: 'Pessoal',
        abas: [
          ['Pão de fermentação natural', 'receitas.exemplo.com'],
          ['Previsão: sábado de sol', 'tempo.exemplo.com'],
          ['Playlist de domingo', 'musica.exemplo.com'],
          ['Ingressos para novembro', 'ingressos.exemplo.com'],
        ],
      },
      {
        nome: 'Trabalho',
        abas: [
          ['Caixa de entrada (12)', 'mail.exemplo.com'],
          ['Custos de outubro', 'planilhas.exemplo.com'],
          ['Notas da reunião semanal', 'docs.exemplo.com'],
          ['Revisão #482: tela de login', 'git.exemplo.com'],
        ],
      },
      {
        nome: 'Estudos',
        abas: [
          ['Hiragana em 15 minutos', 'japones.exemplo.com'],
          ['Cálculo II: lista 3', 'aulas.exemplo.com'],
          ['Resumo: Período Regencial', 'historia.exemplo.com'],
          ['Python: listas por compreensão', 'docs.exemplo.org'],
        ],
      },
    ],
    mostrar: 'Mostrar a lateral',
    esconder: 'Esconder a lateral',
    dica_escondida: 'Agora passe o ponteiro na <b>borda esquerda</b> da janela: a lateral volta por cima da página.',
    copiar: 'Copiar',
    copiado: 'Copiado',
  },
}
