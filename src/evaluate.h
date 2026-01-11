#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

/*
  evaluate.h

  Evaluation API for a C chess engine with optional Stockfish NNUE backend
  via a C ABI wrapper (sf_nnue.h + sf_nnue.cpp compiled as C++).

  Design goals:
    - C-friendly, no Stockfish headers leak into your C code
    - One evaluation context per thread (recommended)
    - You provide a function that converts your Position -> FEN (fast enough to start)
    - Optional: fallback handcrafted eval if NNUE isn't enabled/available
*/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct sf_nnue_ctx sf_nnue_ctx; // just in case
/* Forward-declare your engine's Position type (defined in your position.h). */
typedef struct Position Position;

/* ------------------------ public result types ------------------------ */

typedef struct EvalResult {
    /* Final score from side-to-move perspective, in centipawns (approx). */
    int cp;

    /* If NNUE backend is used, raw Stockfish Value units (PawnValue==208), else 0. */
    int sf_value;

    /* Nonzero if NNUE produced this score. */
    int used_nnue;
} EvalResult;

typedef enum EvalStatus {
    EVAL_OK = 0,
    EVAL_ERR_NULL = -1,
    EVAL_ERR_NNUE_NOT_READY = -2,
    EVAL_ERR_FEN = -3,
    EVAL_ERR_IN_CHECK = -4,
    EVAL_ERR_INTERNAL = -5
} EvalStatus;

/* ------------------------ configuration ------------------------ */

typedef struct EvalConfig {
    /*
      Enable NNUE usage if available.
      If 0, evaluate_position will always call your fallback eval (if set),
      or return 0 cp otherwise.
    */
    int enable_nnue;

    /*
      If you support Chess960, set this per-position (or always 0 for normal chess).
      This is passed to Stockfish Position::set(..., chess960, ...).
    */
    //int chess960;

    /*
      Stockfish "optimism" term. Usually keep 0 unless you intentionally want it.
    */
    int optimism;

    /*
      If nonzero, evaluation module will damp toward 0 as the 50-move rule approaches.
      rule50_plies is taken from a callback you provide.
    */
    int apply_rule50_damping;
} EvalConfig;

/* ------------------------ callbacks you provide ------------------------ */

/*
  Convert Position -> FEN.
  Must write a NUL-terminated string into out_fen (capacity out_cap).
  Return 1 on success, 0 on failure.
*/
typedef int (*pos_to_fen_fn)(const Position* pos, char* out_fen, size_t out_cap);

/*
  Return halfmove clock in plies (0..100). If you store it as "halfmove clock" already,
  just return that. If you store it in full moves, multiply by 2 before returning.
*/
typedef int (*pos_rule50_plies_fn)(const Position* pos);

/*
  Optional handcrafted evaluator fallback.
  Return centipawns from side-to-move perspective.
*/
typedef int (*fallback_eval_fn)(const Position* pos);

/* ------------------------ evaluation context ------------------------ */

/*
  Opaque handle so your C code doesn't need to include Stockfish headers.
  The implementation file (evaluate.c) will include sf_nnue.h when enabled.
*/
typedef struct EvalCtx EvalCtx;

/* Create/destroy an evaluation context (one per thread recommended). */
EvalCtx* eval_create(const EvalConfig* cfg,
                     pos_to_fen_fn to_fen,
                     pos_rule50_plies_fn rule50_plies,
                     fallback_eval_fn fallback);

void eval_destroy(EvalCtx* ctx);

/*
  Load NNUE network(s) for the Stockfish backend.
  - root_dir: used by Stockfish to resolve relative paths (pass "." if unsure)
  - big_evalfile: required .nnue path
  - small_evalfile: optional .nnue path (pass NULL to reuse big)
*/
EvalStatus eval_nnue_load(EvalCtx* ctx,
                          const char* root_dir,
                          const char* big_evalfile,
                          const char* small_evalfile);

/* Evaluate the position. Always returns a result (even if fallback/0). */
EvalStatus evaluate_position(EvalCtx* ctx, const Position* pos, EvalResult* out);

/* Optional: query last backend error string (owned by ctx). */
const char* eval_last_error(const EvalCtx* ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EVALUATE_H_INCLUDED */
