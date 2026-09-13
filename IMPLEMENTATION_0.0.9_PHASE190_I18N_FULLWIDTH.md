# QRX 0.0.9 Phase 190 - Multilingual Apps UX & Full-Width Workspaces

## Scope

Phase 190 closes the UX integration gap introduced by the local QRX App Foundation and QRX Upscaler. It does not change consensus or the 0.0.10 public App Directory design.

## Localization

- Added semantic translation keys for the new `.qrxapp`, App Registry, Developer Mode, permission/install/remove/payment-request, App Host and QRX Upscaler UX.
- All 55 wallet locale catalogs now have the same 885-key schema.
- Dynamic Apps/Upscaler code resolves translations from the active full locale catalog rather than using English-only strings.
- The App Host loads the selected wallet locale and English fallback catalog; Arabic uses RTL direction.
- Product/protocol/format identifiers remain invariant by design (`QRX`, `.qrxapp`, `Mini JS SDK`, `QRX Compute`, `COMPUTE_POUC_V1`, Apple Silicon, Metal, Vulkan and algorithm/runtime names).
- `scripts/qrx-i18n-audit.py` now covers the App Host and validates dynamic `tr()`, `qrxFmt()` and `ht()` translation-key references in addition to static `data-i18n` attributes.

## Full-width layout

- `#view-apps` and `#view-upscaler` are one-column top-level workspaces using 100% of the available content width next to the wallet sidebar.
- Their direct children explicitly span the complete workspace.
- QRX Upscaler uses a two-column internal local/compute split on wide displays and collapses to one column below 900 px.
- The App Host iframe remains 100% x 100% of its host area.
- Dialogs and confirmation surfaces remain intentionally bounded for readability.

## Regression gate

New test: `apps_phase190_i18n_fullwidth`

It checks:

- exactly 55 registered locale catalogs;
- exact key parity against English;
- non-empty Apps/Upscaler/App Host translations in every locale;
- the new localized corpus is not wholesale English fallback;
- static and dynamic new UX translation wiring;
- Apps/Upscaler full-width CSS rules and responsive collapse;
- App Host locale integration and full-frame iframe.

## Documentation

The canonical A-Z handbook now documents the Phase 190 localization/full-width behavior and the verified **123/123** regression baseline. Both root `docs/` and `qrx-core/docs/` copies are synchronized.

## Validation

- i18n parity: **55 locales / 885 keys - PASS**
- editorial-quality gate: **PASS**
- mixed-English lexical audit: **0 candidates in every audited complete locale - PASS**
- JavaScript syntax (`index.html` + App Host inline JS): **PASS**
- Phase 174 + 189 + 190 targeted: **3/3 PASS**
- complete Core regression suite: **123/123 PASS**
- handbook PDF visually rendered/checked after regeneration

Logs:

- `audit/I18N_FULLWIDTH_PHASE190.log`
- `audit/CTEST_0.0.9_PHASE190_123_OF_123.log`
