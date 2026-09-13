from pathlib import Path
root=Path(__file__).resolve().parents[2]
html=(root/'GUIWALLET/src/index.html').read_text()
rs=(root/'GUIWALLET/src-tauri/src/main.rs').read_text()
assert 'Mainnet Health' in html
assert 'mainnetHealthBadge' in html and 'refreshMainnetHealth' in html
assert 'Peer Viewer' in html and 'peerViewerRows' in html
assert 'get_mainnet_health' in rs and '&["getmainnethealth"]' in rs
assert 'get_peer_info' in rs and '&["getpeerinfo"]' in rs
assert 'get_mainnet_health,' in rs and 'get_peer_info,' in rs
assert 'td.textContent=String(text)' in html
assert 'Full peer addresses stay local to this wallet.' in html
print('Phase 7.2.17.1 GUI health/peer viewer assertions: PASS')
