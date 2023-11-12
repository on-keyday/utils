/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <math/fft.h>
#include <wrap/cout.h>

int main() {
    utils::math::fft::test::test_dft();
    constexpr auto N = 1024 * 2;
    utils::math::fft::complex<double> wave[N], dft[N], idft[N];
    constexpr auto pi = 3.141592653589793238462643383279502884197169399375105820974944592307816406286;
    //
    for (int i = 0; i < N; i++) {
        wave[i].real = std::sin(2 * pi * i / N) + std::cos(2 * pi * i / N / 4) + std::sin(i * 2 * pi / N / 8);
        wave[i].imag = 0;
    }
    utils::math::fft::complex_vec sin_wave_vec(wave, N);
    utils::math::fft::complex_vec dft_vec(dft, N);
    utils::math::fft::complex_vec idft_vec(idft, N);
    utils::math::fft::dft(dft_vec, sin_wave_vec);
    utils::math::fft::idft(idft_vec, dft_vec);
    utils::wrap::cout_wrap() << "index,sin_wave(scaled),dft,idft(scaled)\n";
    for (int i = 0; i < N; i++) {
        auto sin_wave_i = wave[i].real;
        auto dft_i = std::sqrt(dft[i].real * dft[i].real + dft[i].imag * dft[i].imag);
        auto idft_i = idft[i].real;
        utils::wrap::cout_wrap() << i << "," << sin_wave_i * 512 << "," << dft_i << "," << idft_i * 512 << "\n";
    }
}