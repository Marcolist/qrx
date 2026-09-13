/* QRX Mini JS SDK v1 - injected into sandboxed .qrxapp iframes. MIT. */
(() => {
  let seq = 0;
  const pending = new Map();
  function call(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = `qrx-${Date.now()}-${++seq}`;
      pending.set(id, { resolve, reject });
      window.parent.postMessage({ type: 'qrx-sdk-request', id, method, params }, '*');
      setTimeout(() => {
        const p = pending.get(id);
        if (p) { pending.delete(id); reject(new Error('QRX SDK request timed out')); }
      }, 15000);
    });
  }
  addEventListener('message', (ev) => {
    const m = ev.data || {};
    if (m.type !== 'qrx-sdk-response' || !m.id || !pending.has(m.id)) return;
    const p = pending.get(m.id); pending.delete(m.id);
    if (m.ok) p.resolve(m.result); else p.reject(new Error(String(m.error || 'QRX SDK call failed')));
  });
  Object.defineProperty(window, 'QRX', { value: Object.freeze({
    version: '1',
    wallet: Object.freeze({
      getIdentity: () => call('wallet.getIdentity'),
      getBalance: () => call('wallet.getBalance'),
      requestPayment: (request) => call('wallet.requestPayment', request || {})
    }),
    chain: Object.freeze({ getHeight: () => call('chain.getHeight') }),
    network: Object.freeze({ getStatus: () => call('network.getStatus') }),
    storage: Object.freeze({
      get: (key) => call('storage.get', { key }),
      set: (key, value) => call('storage.set', { key, value })
    })
  }), writable: false, configurable: false });
  window.dispatchEvent(new CustomEvent('qrx-sdk-ready', { detail: { version: '1' } }));
})();
