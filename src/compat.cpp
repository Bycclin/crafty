// compat.cpp
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>

#include "bitboard.h"
#include "position.h"
#include "types.h"

#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "nnue/nnue_misc.h"

using namespace Stockfish;

namespace {

std::once_flag g_init_once;

void sf_init_stockfish_minimal() {
    // Stockfish does these at startup
    Bitboards::init();
    Position::init();
}

// Mirrors Eval::simple_eval() logic
inline int simple_eval_like_stockfish(const Position& pos) {
    Color c = pos.side_to_move();
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c)) + pos.non_pawn_material(c)
         - pos.non_pawn_material(~c);
}

// Mirrors Eval::use_smallnet()
inline bool use_smallnet_like_stockfish(const Position& pos) {
    return std::abs(simple_eval_like_stockfish(pos)) > 962;
}

// Mirrors Eval::evaluate() scaling/blending path (but without depending on evaluate.cpp/uci.cpp)
inline Value scaled_nnue_like_stockfish(const Eval::NNUE::Networks& networks,
                                        const Position& pos,
                                        Eval::NNUE::AccumulatorStack& accumulators,
                                        Eval::NNUE::AccumulatorCaches& caches,
                                        int optimism) {
    // Same precondition as Stockfish evaluate(): no checkers
    // (If you want, you can handle in-check separately in your engine.)
    bool smallNet = use_smallnet_like_stockfish(pos);

    Value psqt = VALUE_ZERO, positional = VALUE_ZERO;

    if (smallNet)
        std::tie(psqt, positional) = networks.small.evaluate(pos, accumulators, caches.small);
    else
        std::tie(psqt, positional) = networks.big.evaluate(pos, accumulators, caches.big);

    Value nnue = (125 * psqt + 131 * positional) / 128;

    // Re-evaluate with big net when close to 0
    if (smallNet && (std::abs(int(nnue)) < 277)) {
        std::tie(psqt, positional) = networks.big.evaluate(pos, accumulators, caches.big);
        nnue                       = (125 * psqt + 131 * positional) / 128;
        smallNet                   = false;
    }

    // Blend optimism and eval with nnue complexity
    const int nnueComplexity = std::abs(int(psqt - positional));
    optimism += optimism * nnueComplexity / 476;
    nnue -= nnue * nnueComplexity / 18236;

    const int material = 534 * int(pos.count<PAWN>()) + int(pos.non_pawn_material());
    int v              = (int(nnue) * (77871 + material) + optimism * (7191 + material)) / 77871;

    // Rule50 damp
    v -= v * int(pos.rule50_count()) / 199;

    // Avoid TB range
    v = std::clamp(v, int(VALUE_TB_LOSS_IN_MAX_PLY + 1), int(VALUE_TB_WIN_IN_MAX_PLY - 1));
    return Value(v);
}

}  // namespace

extern "C" {

// Opaque context you can hold in your C engine
struct sf_nnue_ctx {
    Eval::NNUE::EvalFile bigFile{};
    Eval::NNUE::EvalFile smallFile{};
    Eval::NNUE::Networks networks{bigFile, smallFile};

    // IMPORTANT: construct AFTER load so cache entries are seeded with loaded biases
    std::unique_ptr<Eval::NNUE::AccumulatorCaches> caches;

    Eval::NNUE::AccumulatorStack accumulators;

    std::string root_dir;
};

sf_nnue_ctx* sf_nnue_create(const char* root_dir) {
    std::call_once(g_init_once, sf_init_stockfish_minimal);

    auto* ctx = new (std::nothrow) sf_nnue_ctx();
    if (!ctx)
        return nullptr;

    ctx->root_dir = root_dir ? root_dir : "";
    return ctx;
}

void sf_nnue_destroy(sf_nnue_ctx* ctx) {
    delete ctx;
}

// Returns 1 on success, 0 on failure
int sf_nnue_load(sf_nnue_ctx* ctx, const char* big_nnue_path, const char* small_nnue_path) {
    if (!ctx || !big_nnue_path || !small_nnue_path)
        return 0;

    try {
        ctx->networks.big.load(ctx->root_dir, std::string(big_nnue_path));
        ctx->networks.small.load(ctx->root_dir, std::string(small_nnue_path));

        // Must be constructed with networks (no default ctor)
        // and should happen after load so biases are correct.
        ctx->caches = std::make_unique<Eval::NNUE::AccumulatorCaches>(ctx->networks);

        ctx->accumulators.reset();
        return 1;
    } catch (...) {
        ctx->caches.reset();
        return 0;
    }
}

// Returns centipawns from the side-to-move POV (Stockfish convention).
// optimism_cp is optional; pass 0 if you don't use it.
int sf_nnue_eval_fen(sf_nnue_ctx* ctx, const char* fen, int optimism_cp) {
    if (!ctx || !ctx->caches || !fen)
        return 0;

    try {
        StateInfo st;
        Position pos;
        pos.set(std::string(fen), /*isChess960=*/false, &st);

        // Stockfish asserts this in Eval::evaluate(); you can decide your own policy.
        if (pos.checkers())
            return 0;

        ctx->accumulators.reset();

        const Value v = scaled_nnue_like_stockfish(ctx->networks, pos, ctx->accumulators, *ctx->caches,
                                                   optimism_cp);
        return int(v);
    } catch (...) {
        return 0;
    }
}

}  // extern "C"
