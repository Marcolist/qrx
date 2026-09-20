from pathlib import Path
r=Path(__file__).resolve().parents[1]
b=(r/'scripts/build-all-targets.sh').read_text()
p=(r/'scripts/prepare-upscaler-ai-bundle.sh').read_text()
c=(r/'qrx-core/src/apps/qrx_upscaler_main.c').read_text()
assert 'mandatory verified local AI Upscaler bundle' in b
assert 'QRX_ALLOW_AI_PENDING' not in b
assert 'AI_UNAVAILABLE marker is forbidden' in b
assert 'git clone --quiet --depth 1 --branch "$RUNTIME_TAG" --recurse-submodules' in p
assert 'CMakeLists.txt missing' in p
assert 'local AI disabled: no usable Vulkan/MoltenVK accelerator' in c
assert '(a.ai_ready&&c.ai_runtime_ready)' in c
print('QRX 0.0.9.94 local scaler capability gating audit: PASS')
