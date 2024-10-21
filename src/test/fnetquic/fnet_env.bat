@echo off
set FNET_LIBSSL=C:/workspace/boringssl/build_debug/ssl/ssl.dll
set FNET_LIBCRYPTO=C:/workspace\boringssl/build_debug/crypto/crypto.dll
set FNET_PUBLIC_KEY=C:/workspace/quic-go-test-server/keys/quic_mock_server.crt
set FNET_PRIVATE_KEY=C:/workspace/quic-go-test-server/keys/quic_mock_server.key
set FNET_NET_CERT=./src/test/fnet_util/cacert.pem
set FNET_ROOT_CA=C:/workspace/shbrgen/brgen_pg_database/ca/certs/20241013145255/root_ca.crt
echo libssl,libcrypto,cert are set
