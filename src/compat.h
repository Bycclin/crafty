#ifndef COMPAT_H
#define COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context (one per thread recommended)
typedef struct sf_nnue_ctx sf_nnue_ctx;

// Error codes (0 = success)
enum {
    SF_NNUE_OK = 0,

    SF_NNUE_ERR_NULL = -1,
    SF_NNUE_ERR_BADARG = -2,
    SF_NNUE_ERR_LOAD = -3,
    SF_NNUE_ERR_FEN = -4,
    SF_NNUE_ERR_IN_CHECK = -5,
    SF_NNUE_ERR_INTERNAL = -6
};

// Call once at startup (idempotent, thread-safe)
int sf_nnue_global_init(void);

// Create/destroy an evaluation context
sf_nnue_ctx* sf_nnue_create(void);
void sf_nnue_destroy(sf_nnue_ctx* ctx);

// Load NNUE network file(s).
// - root_dir: directory used by Stockfish for resolving relative paths (pass "." if unsure)
// - big_evalfile: path to the "big" net (.nnue)
// - small_evalfile: path to the "small" net (.nnue). If NULL/empty, big_evalfile is reused.
int sf_nnue_load_networks(sf_nnue_ctx* ctx,
                          const char* root_dir,
                          const char* big_evalfile,
                          const char* small_evalfile);

// Evaluate a position given by FEN.
// - is_chess960: 0 for normal chess, nonzero for Chess960
// - optimism: pass 0 unless you explicitly want Stockfish's "optimism" term
// Outputs:
// - out_value: Stockfish internal Value units (PawnValue == 208 in current SF) 
// - out_cp: approximate centipawns (rounded), may be NULL if you don’t want it
int sf_nnue_eval_fen(sf_nnue_ctx* ctx,
                     const char* fen,
                     int is_chess960,
                     int optimism,
                     int* out_value,
                     int* out_cp);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SF_NNUE_H_INCLUDED
