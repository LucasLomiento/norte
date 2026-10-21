#include "navegador/browser/ui/links.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace navegador::links {

namespace {

// Parâmetros de rastreio em qualquer site: das redes de anúncio e de
// e-mail marketing (Google, Meta, Microsoft, Mailchimp, HubSpot...). Só os
// que não mudam o conteúdo da página.
constexpr auto kRastreio = base::MakeFixedFlatSet<std::string_view>({
    "fbclid",        "gclid",     "gclsrc",      "dclid",      "gbraid",
    "wbraid",        "msclkid",   "yclid",       "twclid",     "ttclid",
    "li_fat_id",     "mc_cid",    "mc_eid",      "igshid",     "igsh",
    "_hsenc",        "_hsmi",     "__hssc",      "__hstc",     "__hsfp",
    "hsctatracking", "mkt_tok",   "oly_anon_id", "oly_enc_id", "rb_clickid",
    "s_cid",         "vero_conv", "vero_id",     "wickedid",   "_gl",
    "srsltid",       "ref_src",
});

// Os que só rastreiam em alguns sites (noutros, o mesmo nome pode ser da
// página): o "si" de compartilhar do YouTube e do Spotify.
bool RastreioDoSite(std::string_view chave, const GURL& url) {
  if (chave != "si") {
    return false;
  }
  return url.DomainIs("youtube.com") || url.DomainIs("youtu.be") ||
         url.DomainIs("open.spotify.com");
}

bool Rastreio(std::string_view chave, const GURL& url) {
  const std::string minuscula = base::ToLowerASCII(chave);
  return minuscula.starts_with("utm_") || kRastreio.contains(minuscula) ||
         RastreioDoSite(minuscula, url);
}

}  // namespace

GURL Limpar(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || !url.has_query()) {
    return url;
  }
  std::vector<std::string_view> ficam;
  bool tirou = false;
  for (std::string_view par :
       base::SplitStringPiece(url.query(), "&", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    const std::string_view chave = par.substr(0, par.find('='));
    if (Rastreio(chave, url)) {
      tirou = true;
    } else {
      ficam.push_back(par);
    }
  }
  if (!tirou) {
    return url;
  }
  GURL::Replacements trocas;
  const std::string consulta = base::JoinString(ficam, "&");
  if (consulta.empty()) {
    trocas.ClearQuery();
  } else {
    trocas.SetQueryStr(consulta);
  }
  return url.ReplaceComponents(trocas);
}

std::u16string ComoMarkdown(const std::u16string& texto, const GURL& url) {
  std::u16string titulo;
  base::TrimWhitespace(texto, base::TRIM_ALL, &titulo);
  if (titulo.empty()) {
    titulo = base::UTF8ToUTF16(url.spec());
  }
  // Os colchetes do texto e os parênteses do endereço fechariam o link.
  base::ReplaceSubstringsAfterOffset(&titulo, 0, u"\\", u"\\\\");
  base::ReplaceSubstringsAfterOffset(&titulo, 0, u"[", u"\\[");
  base::ReplaceSubstringsAfterOffset(&titulo, 0, u"]", u"\\]");
  std::string endereco = url.spec();
  base::ReplaceSubstringsAfterOffset(&endereco, 0, "(", "%28");
  base::ReplaceSubstringsAfterOffset(&endereco, 0, ")", "%29");
  return u"[" + titulo + u"](" + base::UTF8ToUTF16(endereco) + u")";
}

void Copiar(BrowserWindowInterface* browser, const std::u16string& texto) {
  {
    ui::ScopedClipboardWriter escrever(ui::ClipboardBuffer::kCopyPaste);
    escrever.WriteText(texto);
  }
  if (browser && toast_features::IsEnabled(toast_features::kLinkCopiedToast)) {
    if (ToastController* avisos = ToastController::From(browser)) {
      avisos->MaybeShowToast(ToastParams(ToastId::kLinkCopied));
    }
  }
}

void CopiarEnderecoDaAba(BrowserWindowInterface* browser) {
  content::WebContents* aba =
      browser ? browser->GetTabStripModel()->GetActiveWebContents() : nullptr;
  if (!aba) {
    return;
  }
  Copiar(browser, base::UTF8ToUTF16(Limpar(aba->GetVisibleURL()).spec()));
}

}  // namespace navegador::links
