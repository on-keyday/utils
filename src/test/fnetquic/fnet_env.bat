@echo off
set FNET_QUIC_LIBSSL=C:/workspace/boringssl/build_debug/ssl/ssl.dll
set FNET_QUIC_LIBCRYPTO=C:/workspace\boringssl/build_debug/crypto/crypto.dll
set FNET_QUIC_LOCAL_CERT=C:/workspace/quic-go-test-server/keys/quic_mock_server.crt
set FNET_QUIC_NET_CERT=./src/test/fnet_util/cacert.pem
echo libssl,libcrypto,cert are set
