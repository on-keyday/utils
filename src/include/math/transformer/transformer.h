/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <array>
#include <random>
#include "../matrix.h"
#include "../vector.h"
#include "math/matrix.h"
#include "math/vector.h"

namespace futils::math::transformer {
    template <class Type, size_t word_dim, size_t chunk_dim, size_t N, size_t vocab_c, class Config = DefaultAllocation<DynamicStatusDynamic>>
    struct Transformer {
        static constexpr auto word_dimension = word_dim;
        static constexpr auto chunk_dimension = chunk_dim;
        static_assert(word_dimension % chunk_dimension == 0);
        static constexpr auto chunk_count = word_dimension / chunk_dimension;
        static constexpr auto token_count = N;
        static constexpr auto ffn_dimension = word_dimension * 4;
        static constexpr auto vocab_count = vocab_c;
        using dynamic_allocation = Config;
        using TokenInput = Vector<token_count, size_t, dynamic_allocation>;
        using EmbeddingWeight = Matrix<vocab_count, word_dimension, Type, dynamic_allocation>;
        using WordMat = Matrix<token_count, word_dimension, Type, dynamic_allocation>;
        using AttensionWeight = Matrix<word_dimension, word_dimension, Type, dynamic_allocation>;
        using ChunkedMat = Matrix<token_count, chunk_dimension, Type, dynamic_allocation>;
        using FFNUpMat = Matrix<word_dimension, ffn_dimension, Type, dynamic_allocation>;
        using FFNDownMat = Matrix<ffn_dimension, word_dimension, Type, dynamic_allocation>;
        using SILUMat = Matrix<token_count, ffn_dimension, Type, dynamic_allocation>;
        using QK = Matrix<token_count, token_count, Type, dynamic_allocation>;
        using NormVec = Vector<word_dimension, Type, dynamic_allocation>;
        using VocabWeight = Matrix<word_dimension, vocab_count, Type, dynamic_allocation>;
        using VocabMat = Matrix<token_count, vocab_count, Type, dynamic_allocation>;
        using TokenVec = Vector<token_count, Type, dynamic_allocation>;
        using VocabVec = Vector<vocab_count, Type, dynamic_allocation>;

        struct Weights {
            NormVec norm1;
            NormVec norm2;
            AttensionWeight q;
            AttensionWeight k;
            AttensionWeight v;
            AttensionWeight o;
            FFNUpMat gate;
            FFNUpMat up;
            FFNDownMat down;

            template <class Initializer>
            constexpr void init(Initializer& init) {
                // 1. RMSNorm スケールパラメータ: 1.0 で初期化
                init.init_vector_const(norm1, Type(1));
                init.init_vector_const(norm2, Type(1));

                // 2. Attention 射影行列: Xavier 初期化 (fan_in = D, fan_out = D)
                init.init_xavier(q);
                init.init_xavier(k);
                init.init_xavier(v);
                init.init_xavier(o);

                // 3. FFN (SwiGLU) 行列
                init.init_kaiming(gate);  // fan_in = D, fan_out = 4D (活性化前)
                init.init_kaiming(up);    // fan_in = D, fan_out = 4D
                init.init_xavier(down);   // fan_in = 4D, fan_out = D (活性化後・射影)
            }

            constexpr void sgd(const Weights& w, Type lr) {
                norm1 = norm1 - lr * w.norm1;
                norm2 = norm2 - lr * w.norm2;
                q = q - lr * w.q;
                k = k - lr * w.k;
                v = v - lr * w.v;
                o = o - lr * w.o;
                gate = gate - lr * w.gate;
                up = up - lr * w.up;
                down = down - lr * w.down;
            }
        };

        template <size_t layer>
        struct FullWeights {
            EmbeddingWeight embed;
            std::array<Weights, layer> weights;
            VocabWeight vocab;

            template <class Initializer>
            constexpr void init(Initializer& init) {
                // 埋め込み層および出力語彙射影層
                // stddev = 1 / sqrt(word_dim) で初期化
                Type embed_stddev = Type(1) / std::sqrt(Type(word_dimension));
                init.init_matrix(embed, embed_stddev);
                init.init_matrix(vocab, embed_stddev);

                // 各 Transformer ブロックの初期化
                for (size_t i = 0; i < layer; i++) {
                    weights[i].init(init);
                }
            }

            void sgd(const FullWeights<layer>& v, Type lr) {
                embed = embed - lr * v.embed;
                vocab = vocab - lr * v.vocab;
                for (size_t i = 0; i < layer; i++) {
                    weights[i].sgd(v.weights[i], lr);
                }
            }
        };

        struct InputX {
            WordMat input;
            WordMat norm_1;
            WordMat concat;
            WordMat attension;
            WordMat add;
            WordMat norm_2;
            WordMat ffn;
            WordMat final;
        };

        constexpr static QK soft_max(QK qk) {
            QK sm;
            TokenVec softmax_den;
            ///*
            TokenVec max_val;
            // 1. 各行の最大値を走査
            qk.for_each([&](size_t i, size_t j) {
                if (j == 0 || qk[i][j] > max_val[i]) max_val[i] = qk[i][j];
            });
            //*/
            // 2. 最大値を減算して exp
            qk.for_each([&](size_t i, size_t j) {
                softmax_den[i] += exp(qk[i][j] - max_val[i]);
            });
            // 3. 正規化
            sm.for_each([&](size_t i, size_t j) {
                sm[i][j] = exp(qk[i][j] - max_val[i]) / softmax_den[i];
            });
            return sm;
        }

        constexpr static void causal_mask(QK& raw_score) {
            raw_score.for_each([&](size_t i, size_t j) {
                if (j > i) {
                    raw_score[i][j] = Type(-1e9);
                }
            });
        }

        constexpr static void back_causal_mask(QK& raw_score) {
            raw_score.for_each([&](size_t i, size_t j) {
                if (j > i) {
                    raw_score[i][j] = 0;
                }
            });
        }

        constexpr static QK attention_score(ChunkedMat q, ChunkedMat k) {
            auto raw_score = q * k.T() / sqrt(Type(chunk_dimension));
            causal_mask(raw_score);
            return soft_max(raw_score);
        }

        constexpr static ChunkedMat attention(QK score, ChunkedMat v) {
            return score * v;
        }

        constexpr static QK back_soft_max(QK a, QK da) {
            TokenVec mean;
            a.for_each([&](size_t i, size_t k) {
                mean[i] += da[i][k] * a[i][k];
            });
            QK s;
            s.for_each([&](size_t i, size_t j) {
                s[i][j] = a[i][j] * (da[i][j] - mean[i]);
            });
            return s;
        }

        constexpr static std::tuple<ChunkedMat, ChunkedMat, ChunkedMat> back_attention(ChunkedMat dY, ChunkedMat q, ChunkedMat k, ChunkedMat v) {
            auto a = attention_score(q, k);
            auto d_v = a.T() * dY;
            auto d_a = dY * v.T();
            auto d_s = back_soft_max(a, d_a);
            back_causal_mask(d_s);
            auto d_s_scaled = d_s / sqrt(Type(chunk_dimension));
            auto d_q = d_s_scaled * k;
            auto d_k = d_s_scaled.T() * q;
            return {d_q, d_k, d_v};
        }

        constexpr static std::array<ChunkedMat, chunk_count> chunking(WordMat w) {
            std::array<ChunkedMat, chunk_count> chunked;
            w.for_each([&](size_t i, size_t j) {
                chunked[j / chunk_dimension][i][j % chunk_dimension] = w[i][j];
            });
            return chunked;
        }

        constexpr static WordMat concat(std::array<ChunkedMat, chunk_count> c) {
            WordMat w;
            w.for_each([&](size_t i, size_t j) {
                w[i][j] = c[j / chunk_dimension][i][j % chunk_dimension];
            });
            return w;
        }

        constexpr static WordMat multihead_attention(WordMat x, AttensionWeight wq, AttensionWeight wk, AttensionWeight wv) {
            auto q = x * wq;
            auto k = x * wk;
            auto v = x * wv;
            auto chunked_q = chunking(q);
            auto chunked_k = chunking(k);
            auto chunked_v = chunking(v);
            std::array<ChunkedMat, chunk_count> attentions;
            for (size_t i = 0; i < chunk_count; i++) {
                apply_rope(chunked_q[i]);
                apply_rope(chunked_k[i]);
                attentions[i] = attention(attention_score(chunked_q[i], chunked_k[i]), chunked_v[i]);
            }
            return concat(attentions);
        }

        constexpr static void apply_rope(ChunkedMat& x) {
            for (size_t m = 0; m < token_count; m++) {  // 系列方向のインデックス
                for (size_t i = 0; i < chunk_dimension / 2; i++) {
                    Type theta = std::pow(Type(10000), -Type(2 * i) / Type(chunk_dimension));
                    Type angle = Type(m) * theta;
                    Type cos_val = std::cos(angle);
                    Type sin_val = std::sin(angle);

                    Type x0 = x[m][2 * i];
                    Type x1 = x[m][2 * i + 1];

                    x[m][2 * i] = x0 * cos_val - x1 * sin_val;
                    x[m][2 * i + 1] = x0 * sin_val + x1 * cos_val;
                }
            }
        }

        // 逆伝播: d_chunked_q, d_chunked_k に対して適用 (sinの符号反転)
        constexpr static void back_rope(ChunkedMat& dx) {
            for (size_t m = 0; m < N; m++) {
                for (size_t i = 0; i < chunk_dim / 2; i++) {
                    Type theta = std::pow(Type(10000), -Type(2 * i) / Type(chunk_dim));
                    Type angle = Type(m) * theta;
                    Type cos_val = std::cos(angle);
                    Type sin_val = std::sin(angle);

                    Type g0 = dx[m][2 * i];
                    Type g1 = dx[m][2 * i + 1];

                    dx[m][2 * i] = g0 * cos_val + g1 * sin_val;
                    dx[m][2 * i + 1] = -g0 * sin_val + g1 * cos_val;
                }
            }
        }

        constexpr static WordMat back_multihead_attention(WordMat g, WordMat x_concat, WordMat input_x, const Weights& w, Weights& updates) {
            auto q = input_x * w.q;
            auto k = input_x * w.k;
            auto v = input_x * w.v;
            auto chunked_q = chunking(q);
            auto chunked_k = chunking(k);
            auto chunked_v = chunking(v);

            auto d_wo = x_concat.T() * g;
            auto d_x_concat = g * w.o.T();
            auto chunked_attension = chunking(d_x_concat);

            std::array<ChunkedMat, chunk_count> chunked_dq, chunked_dk, chunked_dv;

            for (size_t i = 0; i < chunk_count; i++) {
                apply_rope(chunked_q[i]);  // ← 順伝播と同じ回転をかけてから
                apply_rope(chunked_k[i]);
                auto [dq, dk, dv] = back_attention(chunked_attension[i], chunked_q[i], chunked_k[i], chunked_v[i]);
                back_rope(dq);
                back_rope(dk);
                chunked_dq[i] = dq;
                chunked_dk[i] = dk;
                chunked_dv[i] = dv;
            }

            auto dq = concat(chunked_dq);
            auto dk = concat(chunked_dk);
            auto dv = concat(chunked_dv);
            updates.q = input_x.T() * dq;
            updates.k = input_x.T() * dk;
            updates.v = input_x.T() * dv;
            updates.o = d_wo;

            auto dx = dq * w.q.T() + dk * w.k.T() + dv * w.v.T();
            return dx;
        }

        constexpr static WordMat rms_norm(WordMat x, NormVec weight) {
            TokenVec rms;
            x.for_each([&](size_t i, size_t j) {
                rms[i] += x[i][j] * x[i][j];
            });
            for (auto& d : rms) {
                d /= Type(word_dimension);
                d += Type(1e-6);
                d = sqrt(d);
            }
            WordMat result;
            x.for_each([&](size_t i, size_t j) {
                result[i][j] = x[i][j] / rms[i] * weight[j];
            });
            return result;
        }

        constexpr static WordMat back_rms_norm(WordMat g, WordMat x, NormVec weight, NormVec& update) {
            TokenVec rms;
            x.for_each([&](size_t i, size_t j) {
                rms[i] += x[i][j] * x[i][j];
            });
            for (auto& d : rms) {
                d /= Type(word_dimension);
                d += Type(1e-6);
                d = sqrt(d);
            }
            NormVec d_weight;
            g.for_each([&](size_t i, size_t j) {
                d_weight[j] += g[i][j] * (x[i][j] / rms[i]);
            });
            WordMat dx_bar;
            dx_bar.for_each([&](size_t i, size_t j) {
                dx_bar[i][j] = g[i][j] * weight[j];
            });
            TokenVec c_vec;
            dx_bar.for_each([&](size_t i, size_t k) {
                c_vec[i] += dx_bar[i][k] * (x[i][k] / rms[i]);
            });
            for (auto& c : c_vec) {
                c /= Type(word_dimension);
            }
            WordMat dx;
            dx.for_each([&](size_t i, size_t j) {
                dx[i][j] = (dx_bar[i][j] - c_vec[i] * (x[i][j] / rms[i])) / rms[i];
            });

            update = d_weight;
            return dx;
        }

        constexpr static Type sigmoid(Type x) {
            return 1 / (1 + exp(-x));
        }

        constexpr static SILUMat silu(SILUMat z) {
            SILUMat r;
            z.for_each([&](size_t i, size_t j) {
                r[i][j] = z[i][j] * sigmoid(z[i][j]);
            });
            return r;
        }

        constexpr static SILUMat silu_derived(SILUMat x) {
            SILUMat r;
            x.for_each([&](size_t i, size_t j) {
                auto sig = sigmoid(x[i][j]);
                r[i][j] = sig * (1 + x[i][j] * (1 - sig));
            });
            return r;
        }

        constexpr static WordMat ffn(WordMat x, FFNUpMat w_gate, FFNUpMat w_up, FFNDownMat w_down) {
            auto gate = x * w_gate;
            auto up = x * w_up;
            auto act_gate = silu(gate);
            auto hidden = hadamard(act_gate, up);
            return hidden * w_down;
        }

        constexpr static WordMat back_ffn(WordMat g, WordMat x, FFNUpMat w_gate, FFNUpMat w_up, FFNDownMat w_down, Weights& update) {
            auto gate = x * w_gate;
            auto up = x * w_up;
            auto act_gate = silu(gate);
            auto hidden = hadamard(act_gate, up);

            auto d_w_down = hidden.T() * g;
            auto g_hidden = g * w_down.T();
            auto g_up = hadamard(g_hidden, act_gate);
            auto g_act = hadamard(g_hidden, up);
            auto g_gate = hadamard(g_act, silu_derived(gate));

            auto d_w_up = x.T() * g_up;
            auto d_w_gate = x.T() * g_gate;
            auto d_x = g_up * w_up.T() + g_gate * w_gate.T();

            update.gate = d_w_gate;
            update.up = d_w_up;
            update.down = d_w_down;

            return d_x;
        }

        constexpr static WordMat run_step(WordMat x,
                                          const Weights& w, InputX& input) {
            WordMat x_norm = rms_norm(x, w.norm1);
            WordMat x_attention_concat = multihead_attention(x_norm, w.q, w.k, w.v);
            WordMat x_attention = x_attention_concat * w.o;
            WordMat x_add = x + x_attention;
            WordMat x_norm2 = rms_norm(x_add, w.norm2);
            WordMat x_ffn = ffn(x_norm2, w.gate, w.up, w.down);
            WordMat x_final = x_add + x_ffn;
            input.input = x;
            input.norm_1 = x_norm;
            input.concat = x_attention_concat;
            input.attension = x_attention;
            input.add = x_add;
            input.norm_2 = x_norm2;
            input.ffn = x_ffn;
            input.final = x_final;
            return x_final;
        }

        constexpr static WordMat back_step(
            WordMat g_final, const Weights& w, Weights& update, const InputX& input) {
            WordMat g_bypass = g_final;
            WordMat g_ffn = g_final;
            WordMat g_norm2 = back_ffn(g_ffn, input.norm_2, w.gate, w.up, w.down, update);
            WordMat g_add = back_rms_norm(g_norm2, input.add, w.norm2, update.norm2);
            WordMat g_mid = g_bypass + g_add;
            WordMat g_bypass2 = g_mid;
            WordMat g_attension_out = g_mid;
            WordMat g_norm1 = back_multihead_attention(g_attension_out, input.concat, input.norm_1, w, update);
            WordMat g_input_norm = back_rms_norm(g_norm1, input.input, w.norm1, update.norm1);
            WordMat g_input = g_mid + g_input_norm;
            return g_input;
        }

        constexpr static VocabMat logit(WordMat x, VocabWeight v) {
            return x * v;
        }

        constexpr static VocabMat back_logit(VocabMat p, TokenInput answer) {
            VocabMat dz;
            dz.for_each([&](size_t t, size_t v) {
                dz[t][v] = p[t][v] / Type(token_count);
            });
            for (size_t t = 0; t < token_count; t++) {
                dz[t][answer[t]] -= Type(1) / Type(token_count);
            }
            return dz;
        }

        template <size_t w>
        constexpr static WordMat back_start(WordMat x_final, VocabWeight w_vocab, VocabMat dz, FullWeights<w>& update) {
            VocabWeight d_w_vocab = x_final.T() * dz;
            WordMat g_final = dz * w_vocab.T();
            update.vocab = d_w_vocab;
            return g_final;
        }

        constexpr static VocabMat probability(VocabMat x) {
            TokenVec max_val;
            TokenVec dens;

            // 1. 各行（各トークン）の最大 logit を走査
            x.for_each([&](size_t t, size_t v) {
                if (v == 0 || x[t][v] > max_val[t]) {
                    max_val[t] = x[t][v];
                }
            });

            // 2. 最大値を引いて exp() を加算（オーバーフロー完全遮断）
            x.for_each([&](size_t t, size_t v) {
                dens[t] += std::exp(x[t][v] - max_val[t]);
            });

            // 3. ゼロ除算防御を挟んで確率化
            VocabMat p;
            x.for_each([&](size_t t, size_t v) {
                Type den = dens[t];
                if (den < Type(1e-12)) den = Type(1e-12);
                p[t][v] = std::exp(x[t][v] - max_val[t]) / den;
            });
            return p;
        }

        constexpr static Type cross_entropy(VocabMat p, TokenInput answer) {
            Type sum{};
            for (size_t t = 0; t < token_count; t++) {
                Type prob = p[t][answer[t]];
                sum += log(prob);
            }
            return -1 * sum / Type(token_count);
        }

        constexpr static WordMat embed_lookup(
            const TokenInput& token_ids,
            const EmbeddingWeight& embed) {
            WordMat x;
            for (size_t t = 0; t < token_count; t++) {
                size_t id = token_ids[t];
                for (size_t d = 0; d < word_dimension; d++) {
                    x[t][d] = embed[id][d];
                }
            }
            return x;
        }

        template <size_t layer>
        constexpr static void back_embed(
            WordMat g_input,
            const TokenInput& token_ids,
            FullWeights<layer>& update) {
            // update.embed はゼロ初期化されている前提
            for (size_t t = 0; t < token_count; t++) {
                size_t id = token_ids[t];
                for (size_t d = 0; d < word_dimension; d++) {
                    update.embed[id][d] += g_input[t][d];
                }
            }
        }

        template <size_t layer>
        constexpr static std::tuple<WordMat, VocabMat, Type> forward(const FullWeights<layer>& w,
                                                                     std::array<InputX, layer>& input,
                                                                     const TokenInput& input_ids,
                                                                     const TokenInput& target_ids) {
            auto x = embed_lookup(input_ids, w.embed);
            for (size_t i = 0; i < layer; i++) {
                x = run_step(x, w.weights[i], input[i]);
            }
            auto logits = logit(x, w.vocab);
            auto p = probability(logits);
            return {x, p, cross_entropy(p, target_ids)};
        }

        template <size_t layer>
        constexpr static FullWeights<layer> backward(const FullWeights<layer>& w,
                                                     const std::array<InputX, layer>& input,
                                                     VocabMat& p,
                                                     const TokenInput& input_ids,
                                                     const TokenInput& target_ids, const WordMat& final_input) {
            VocabMat dz = back_logit(p, target_ids);
            FullWeights<layer> update;
            WordMat next_back = back_start(final_input, w.vocab, dz, update);

            for (size_t i = 0; i < layer; i++) {
                next_back = back_step(next_back, w.weights[layer - 1 - i], update.weights[layer - 1 - i], input[layer - 1 - i]);
            }
            back_embed(next_back, input_ids, update);
            return update;
        }

        template <class MatrixT>
        constexpr static void check_matrix(
            MatrixT& mat, const MatrixT& analytic, const char* name, size_t layer,
            size_t report_limit,
            auto&& loss_fn, Type eps = 1e-5, Type border = 1e-4) {
            Type worst = 0;
            size_t count = 0;

            auto logic = [&](auto&& access, auto&& analys, auto&&... args) {
                const auto original = access();
                access() = original + eps;
                const auto lp = loss_fn();
                access() = original - eps;
                const auto lm = loss_fn();
                access() = original;

                const auto num = (lp - lm) / (2 * eps);
                const auto ana = analys();
                const auto denom = std::max(1e-12, abs(num) + abs(ana));
                const auto rel = abs(num - ana) / denom;
                if (rel > border && count < report_limit) {
                    observe_access<Config>(ObservePoint::report, layer, name, lp, lm, num, ana, rel, args...);
                    count++;
                }
                worst = std::max(worst, rel);
            };

            if constexpr (MatrixT::dimention == 2) {
                mat.for_each([&](size_t i, size_t j) {
                    logic([&]() -> Type& { return mat[i][j]; }, [&]() -> const Type& { return analytic[i][j]; }, i, j);
                });
            }
            else if constexpr (MatrixT::dimention == 1) {
                mat.for_each([&](size_t i) {
                    logic([&]() -> Type& { return mat[i]; }, [&]() -> const Type& { return analytic[i]; }, i);
                });
            }
            observe_access<Config>(ObservePoint::report, layer, name, worst);
        }

        template <size_t layer>
        constexpr static Type train_step(
            TokenInput input_ids,   // 入力トークン埋め込み列 (N x D)
            TokenInput target_ids,  // 正解トークンID配列 (N)
            FullWeights<layer>& w,  // Transformer重み
            Type learning_rate,     // 学習率
            size_t matrix_check = 0) {
            std::array<InputX, layer> input;
            auto [next_input, p, loss] = forward(w, input, input_ids, target_ids);
            auto update = backward(w, input, p, input_ids, target_ids, next_input);
            if (matrix_check) {
                auto L = [&] {
                    auto [_0, _1, loss] = forward(w, input, input_ids, target_ids);
                    return loss;
                };

#define CHECK(path, l)                                      \
    observe_access<Config>(ObservePoint::report, l, #path); \
    check_matrix(w.path, update.path, #path, l, matrix_check, L)

                CHECK(vocab, layer + 1);
                for (size_t i = 0; i < layer; i++) {
                    size_t l = layer - 1 - i;
                    CHECK(weights[l].down, l + 1);
                    CHECK(weights[l].up, l + 1);
                    CHECK(weights[l].gate, l + 1);
                    CHECK(weights[l].norm2, l + 1);
                    CHECK(weights[l].o, l + 1);
                    CHECK(weights[l].v, l + 1);
                    CHECK(weights[l].k, l + 1);
                    CHECK(weights[l].q, l + 1);
                    CHECK(weights[l].norm1, l + 1);
                }
                CHECK(embed, 0);

#undef CHECK
            }
            w.sgd(update, learning_rate);

            return loss;
        }

        constexpr static size_t sample_discrete(VocabVec& p, auto& rng) {
            std::uniform_real_distribution<Type> dist;
            Type r = dist(rng);
            Type cumsum = Type(0);

            for (size_t v = 0; v < vocab_count; v++) {
                cumsum += p[v];
                if (r <= cumsum) {
                    return v;
                }
            }
            return vocab_count - 1;  // 浮動小数点丸め誤差防止
        }

        // Temperature スケーリング付き Softmax
        constexpr static VocabVec softmax_with_temperature(
            const VocabVec& logits,
            Type temperature) {
            VocabVec p;
            Type max_logit = logits[0] / temperature;
            for (size_t v = 1; v < vocab_count; v++) {
                Type val = logits[v] / temperature;
                if (val > max_logit) max_logit = val;
            }

            Type sum = Type(0);
            for (size_t v = 0; v < vocab_count; v++) {
                p[v] = std::exp(logits[v] / temperature - max_logit);  // オーバーフロー防止
                sum += p[v];
            }
            for (size_t v = 0; v < vocab_count; v++) {
                p[v] /= sum;
            }
            return p;
        }

        template <size_t layer>
        static constexpr void generate_with_prefix(
            auto&& buffer,
            const FullWeights<layer>& w, Type temperature, auto& rng, size_t pad_id, size_t eos_id, auto&& fill_prefix, auto&& map_id_to_token) {
            TokenInput input_ids;
            for (size_t i = 0; i < token_count; i++) {
                input_ids[i] = pad_id;
            }
            input_ids[0] = eos_id;

            size_t start_t = fill_prefix(buffer, input_ids);

            for (size_t t = start_t; t < token_count - 1; t++) {
                WordMat x = embed_lookup(input_ids, w.embed);
                WordMat next_input = x;
                std::array<InputX, layer> dummy_input{};
                for (size_t i = 0; i < layer; i++) {
                    next_input = run_step(next_input, w.weights[i], dummy_input[i]);
                }
                VocabMat logits = logit(next_input, w.vocab);
                size_t sampled_id = 0;
                if (temperature == 0) {
                    VocabMat p = probability(logits);
                    size_t best_id = 0;
                    Type max_prob = p[t][0];
                    for (size_t v = 1; v < vocab_count; v++) {
                        if (p[t][v] > max_prob) {
                            max_prob = p[t][v];
                            best_id = v;
                        }
                    }
                    sampled_id = best_id;
                }
                else {
                    VocabVec current_logits;
                    for (size_t v = 0; v < vocab_count; v++) {
                        current_logits[v] = logits[t][v];
                    }

                    // 3. 温度スケーリング & 確率分布計算
                    VocabVec p = softmax_with_temperature(current_logits, temperature);

                    sampled_id = sample_discrete(p, rng);
                }

                if (sampled_id == eos_id || sampled_id == pad_id) {
                    break;
                }

                map_id_to_token(buffer, sampled_id);
                input_ids[t + 1] = sampled_id;
            }
        }
    };

}  // namespace futils::math::transformer
