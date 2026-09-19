import hashlib,pathlib,sys
D=pathlib.Path(sys.argv[1]); target=sys.argv[2] if len(sys.argv)>2 else 'host'
R=D/'runtime'/('realesrgan-ncnn-vulkan.exe' if target.startswith('windows') else 'realesrgan-ncnn-vulkan')
need=[R,D/'runtime/runtime.sha256',D/'models/realesr-animevideov3-x2.param',D/'models/realesr-animevideov3-x2.bin',D/'models/realesrgan-x2plus.qrxmodel',D/'models/realesrgan-x4plus.param',D/'models/realesrgan-x4plus.bin',D/'models/realesrgan-x4plus.qrxmodel',D/'PROVENANCE.txt']
for f in need:
    if not f.is_file(): raise SystemExit('AI bundle verification failed: missing '+str(f))
def h(p): return hashlib.sha256(p.read_bytes()).hexdigest()
expected=(D/'runtime/runtime.sha256').read_text().strip().lower()
assert len(expected)==64 and all(c in '0123456789abcdef' for c in expected), 'invalid runtime hash lock'
assert h(R)==expected, 'runtime SHA-256 mismatch'
for mf in ['realesrgan-x2plus.qrxmodel','realesrgan-x4plus.qrxmodel']:
    vals={}
    for line in (D/'models'/mf).read_text().splitlines():
        if '=' in line:
            k,v=line.split('=',1); vals[k]=v
    assert vals.get('format')=='qrx-upscaler-model-v2', f'{mf}: bad format'
    for name,key in [('param','param_sha256'),('weights','weights_sha256')]:
        p=D/'models'/vals[name]; assert p.is_file(), f'{mf}: missing {p.name}'; assert h(p)==vals[key].lower(), f'{mf}: {name} SHA-256 mismatch'
print('QRX verified AI bundle audit: PASS')
