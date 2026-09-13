# AURA loopback HTTPS fixture certificate

`localhost-cert.pem` and `localhost-key.pem` are a **test-only**, self-signed
certificate/key pair used exclusively by `aura_https_fixture_runner.py` on
127.0.0.1 during CTest. They are not a QRX trust root, release key, governance
key, model-publisher key or production credential.

The production AURA origin downloader remains HTTPS-only and uses the platform
trust store unless an explicit `tls_ca_bundle` is configured.
