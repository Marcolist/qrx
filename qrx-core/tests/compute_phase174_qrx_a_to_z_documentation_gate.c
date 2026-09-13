#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
  const char *view;
  const char *required_text;
} DocViewRequirement;

static char *read_all(const char *path) {
  FILE *f = fopen(path, "rb");
  long n;
  char *buf;
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  n = ftell(f);
  if (n < 0) { fclose(f); return NULL; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
  buf = (char*)malloc((size_t)n + 1u);
  if (!buf) { fclose(f); return NULL; }
  if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf); fclose(f); return NULL;
  }
  buf[n] = '\0';
  fclose(f);
  return buf;
}

static const char *required_for_view(const char *view) {
  static const DocViewRequirement reqs[] = {
    {"dashboard", "Dashboard"},
    {"apps", "Apps"},
    {"upscaler", "QRX Upscaler"},
    {"drive", "QRX Drive"},
    {"qrxnet", "QRX-Net"},
    {"resource", "Resource Globe"},
    {"send", "Send"},
    {"receive", "Receive"},
    {"staking", "Staking"},
    {"validator", "Validator Mode"},
    {"transactions", "Transactions"},
    {"btc", "BTC Light"},
    {"swaps", "Quantum Swaps"},
    {"agents", "Agents & Kraken"},
    {"privacy", "Privacy"},
    {"safety", "Safety Center"},
    {"roadmap", "Roadmap"},
    {"wallets", "Wallets"},
    {"markets", "Markets"},
    {"addressbook", "Address Book"},
    {"advanced", "Advanced provider controls"}
  };
  size_t i;
  for (i = 0; i < sizeof(reqs)/sizeof(reqs[0]); ++i) {
    if (strcmp(reqs[i].view, view) == 0) return reqs[i].required_text;
  }
  return NULL;
}

static int contains(const char *haystack, const char *needle) {
  return haystack && needle && strstr(haystack, needle) != NULL;
}

int main(int argc, char **argv) {
  const char *html_path;
  const char *doc_path;
  char *html = NULL;
  char *doc = NULL;
  const char *p;
  int failures = 0;
  int views = 0;
  const char *mandatory[] = {
    "Installation and first start",
    "Wallet setup",
    "Recovery Center",
    "QRX Drive",
    "QRX-Net",
    "AURA — beginner guide",
    "CLI and RPC",
    "Security model",
    "Troubleshooting",
    "Glossary"
  };
  size_t i;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <GUIWALLET/src/index.html> <QRX_A_TO_Z_0.0.9.md>\n", argv[0]);
    return 2;
  }
  html_path = argv[1];
  doc_path = argv[2];
  html = read_all(html_path);
  doc = read_all(doc_path);
  if (!html || !doc) {
    fprintf(stderr, "phase174: unable to read GUI or handbook\n");
    free(html); free(doc);
    return 3;
  }

  p = html;
  while ((p = strstr(p, "id=\"view-")) != NULL) {
    char view[80];
    size_t n = 0;
    const char *s = p + strlen("id=\"view-");
    const char *required;
    while (*s && *s != '\"' && n + 1 < sizeof(view)) view[n++] = *s++;
    view[n] = '\0';
    if (*s != '\"' || n == 0) {
      fprintf(stderr, "phase174: malformed view id near offset %ld\n", (long)(p - html));
      ++failures;
      p += 4;
      continue;
    }
    ++views;
    required = required_for_view(view);
    if (!required) {
      fprintf(stderr, "phase174: GUI view '%s' has no documentation mapping\n", view);
      ++failures;
    } else if (!contains(doc, required)) {
      fprintf(stderr, "phase174: GUI view '%s' requires handbook text '%s'\n", view, required);
      ++failures;
    }
    p = s + 1;
  }

  if (views < 20) {
    fprintf(stderr, "phase174: expected at least 20 GUI views, found %d\n", views);
    ++failures;
  }

  for (i = 0; i < sizeof(mandatory)/sizeof(mandatory[0]); ++i) {
    if (!contains(doc, mandatory[i])) {
      fprintf(stderr, "phase174: mandatory handbook section missing: %s\n", mandatory[i]);
      ++failures;
    }
  }

  if (!contains(doc, "106/106") && !contains(doc, "107/107") && !contains(doc, "108/108") && !contains(doc, "109/109") && !contains(doc, "110/110") && !contains(doc, "111/111") && !contains(doc, "112/112") && !contains(doc, "113/113") && !contains(doc, "114/114") && !contains(doc, "115/115") && !contains(doc, "116/116") && !contains(doc, "117/117") && !contains(doc, "118/118") && !contains(doc, "119/119") && !contains(doc, "120/120") && !contains(doc, "121/121") && !contains(doc, "122/122") && !contains(doc, "123/123")) {
    fprintf(stderr, "phase174: handbook must state the verified regression baseline\n");
    ++failures;
  }

  free(html);
  free(doc);
  if (failures) {
    fprintf(stderr, "phase174: FAIL (%d issue%s)\n", failures, failures == 1 ? "" : "s");
    return 1;
  }
  printf("phase174: PASS - %d GUI views plus mandatory A-Z sections are documented\n", views);
  return 0;
}
