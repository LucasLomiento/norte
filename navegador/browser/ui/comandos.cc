#include "navegador/browser/ui/comandos.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/espacos.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace navegador::comandos {

namespace {

// Palavras a mais que acham o comando ("> sidebar", "> split").
std::u16string Palavras(std::string_view acao) {
  if (acao == "alternar_lateral") {
    return u"abas sidebar barra lateral";
  }
  if (acao == "alternar_barra") {
    return u"url endereco toolbar";
  }
  if (acao == "modo_foco") {
    return u"zen compacto tela";
  }
  if (acao == "dividir_lado" || acao == "dividir_cima" ||
      acao == "trocar_lados") {
    return u"split divisao";
  }
  if (acao == "copiar_endereco") {
    return u"link url limpo";
  }
  if (acao == "fixar_no_espaco" || acao == "favoritar") {
    return u"pin";
  }
  if (acao == "proximo_espaco" || acao == "espaco_anterior" ||
      acao == "novo_espaco" || acao == "fechar_espaco") {
    return u"workspace";
  }
  if (acao == "configuracoes") {
    return u"opcoes preferencias settings";
  }
  return std::u16string();
}

// A janela ativa do perfil (a da barra em que o comando foi escolhido).
BrowserWindowInterface* Janela(Profile* perfil) {
  BrowserWindowInterface* achada = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* janela) {
        if (janela->GetProfile() == perfil) {
          achada = janela;
        }
        return !achada;
      });
  return achada;
}

}  // namespace

std::vector<NavegadorComando> Lista(Profile* perfil) {
  std::vector<NavegadorComando> lista;
  const bool anonima = perfil->IsOffTheRecord();
  for (const espacos::Atalho& atalho : espacos::Atalhos()) {
    // "espaco_1" a "espaco_9" viram um comando por espaço, pelo nome (o
    // "espaco_anterior" fica).
    const bool espaco_numerado = atalho.acao.size() == 8 &&
                                 atalho.acao.starts_with("espaco_") &&
                                 base::IsAsciiDigit(atalho.acao.back());
    if (espaco_numerado || (anonima && espacos::AcaoDeEspaco(atalho.acao))) {
      continue;
    }
    lista.push_back({atalho.acao, atalho.descricao, Palavras(atalho.acao)});
  }
  lista.push_back(
      {"fechar_repetidas", u"Fechar as abas repetidas", u"duplicadas"});
  lista.push_back(
      {"copiar_enderecos", u"Copiar os endereços das abas", u"links url"});
  if (anonima) {
    return lista;
  }
  lista.push_back({"descarregar_outros", u"Descarregar os outros espaços",
                   u"memoria workspace"});
  Profile* original = perfil->GetOriginalProfile();
  for (const espacos::Espaco& espaco : espacos::Todos(original)) {
    lista.push_back({"espaco:" + espaco.id,
                     u"Ir pro espaço " + base::UTF8ToUTF16(espaco.nome),
                     u"workspace"});
  }
  for (const containers::Container& container :
       containers::GetAll(original)) {
    if (container.temporario) {
      continue;
    }
    lista.push_back({"container:" + container.id,
                     u"Nova aba no container " +
                         base::UTF8ToUTF16(container.name),
                     u"container"});
  }
  lista.push_back({"temporario", u"Nova aba num container temporário",
                   u"container"});
  return lista;
}

void Executar(Profile* perfil, const std::string& id) {
  BrowserWindowInterface* janela = Janela(perfil);
  if (id.starts_with("copiar:")) {
    {
      ui::ScopedClipboardWriter escrever(ui::ClipboardBuffer::kCopyPaste);
      escrever.WriteText(base::UTF8ToUTF16(id.substr(7)));
    }
    if (ToastController* avisos =
            janela ? ToastController::From(janela) : nullptr) {
      avisos->MaybeShowToast(ToastParams(ToastId::kCopiedToClipboard));
    }
    return;
  }
  if (janela) {
    espacos::ExecutarAcao(janela, id);
  }
}

}  // namespace navegador::comandos
