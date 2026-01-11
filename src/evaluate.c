#include "chess.h"
#include "evaluate.h"
#include "data.h"

struct EvalCtx {
    EvalConfig cfg;

    pos_to_fen_fn to_fen;
    pos_rule50_plies_fn rule50_plies;
    fallback_eval_fn fallback;

    sf_nnue_ctx* nnue;
    int nnue_loaded;

    char last_error[256];
};

static void set_err(EvalCtx* ctx, const char* msg) {
    if (!ctx) return;
    if (!msg) msg = "unknown error";
    snprintf(ctx->last_error, sizeof(ctx->last_error), "%s", msg);
}

//static int clamp_i(int v, int lo, int hi) {
//    if (v < lo) return lo;
//    if (v > hi) return hi;
//    return v;
//}
static int is_mate_like(int cp) {
    return (cp >= 29000) || (cp <= -29000);
}
//static int apply_rule50_scale_cp(int cp, int rule50_plies) {
//    rule50_plies = clamp_i(rule50_plies, 0, 100);
//    const int scale = 100 - rule50_plies;
//    if (cp >= 0)
//        return (cp * scale + 50) / 100;
//    else
//        return -(((-cp) * scale + 50) / 100);
//}

static void fill_result(EvalResult* out, int cp, int sf_value, int used_nnue) {
    if (!out) return;
    out->cp = cp;
    out->sf_value = sf_value;
    out->used_nnue = used_nnue;
}

int Evaluate(TREE * tree, int ply, int wtm, int alpha, int beta) {
  PAWN_HASH_ENTRY *ptable;
  PXOR *pxtable;
  int score, side, can_win = 3, phase, lscore, cutoff;

#if defined(ELO)
  if (nps_loop) {
    int i, j;
    for (i = 0; i < nps_loop && !abort_search; i++)
      for (j = 1; j < 10 && !abort_search; j++)
        burner[j - 1] = burner[j - 1] * burner[j];
    if (TimeCheck(tree, 1))
      abort_search = 1;
  }
#endif
  //if (!ctx->nnue || !ctx->nnue_loaded)
    //    return fallback_eval(EVAL_ERR_NNUE_NOT_READY, "NNUE requested but networks not loaded");

    //if (!ctx->to_fen)
    //    return fallback_eval(EVAL_ERR_FEN, "to_fen callback not provided");

    char fen[140];
    fen[0] = '\0';

    //if (!ctx->to_fen(pos, fen, sizeof(fen))) // TODO: not use FEN...
    //    return fallback_eval(EVAL_ERR_FEN, "to_fen failed");

    int sf_value = 0;
    int cp = 0;

    const int rc = sf_nnue_eval_fen(global_nnue_instance, fen, 0, 100, &sf_value, &cp);
    int scaled_cp = rc * PAWN_VALUE / 100; //should be more specific than 100
    //if (rc != SF_NNUE_OK) {
    //    if (rc == SF_NNUE_ERR_IN_CHECK)
    //        return fallback_eval(EVAL_ERR_IN_CHECK, "SF NNUE eval refused position: side to move is in check");
    //    if (rc == SF_NNUE_ERR_FEN)
    //        return fallback_eval(EVAL_ERR_FEN, "SF NNUE eval failed: bad FEN");
    //    return fallback_eval(EVAL_ERR_INTERNAL, "SF NNUE eval failed");
    //}
    //if (ctx->cfg.apply_rule50_damping && ctx->rule50_plies && !is_mate_like(cp)) {
    //    const int r50 = ctx->rule50_plies(pos);
    //    cp = apply_rule50_scale_cp(cp, r50);
    //}
    //ctx->last_error[0] = '\0';
    fill_result(out, scaled_cp, sf_value, 1);
    //return EVAL_OK;
    return cp;
#if defined(ELO)
  if (elo_randomize)
    score =
        elo_randomize * score / 100 + ((100 -
            elo_randomize) * PAWN_VALUE * (Random32() % 100)) / 10000;
#endif
  //return (wtm) ? score : -score;
}

//void EvaluateMaterial(TREE * tree, int wtm) {
//  int score_mg, score_eg, majors, minors;
//  score_mg = Material + ((wtm) ? wtm_bonus[mg] : -wtm_bonus[mg]);
//  score_eg = Material + ((wtm) ? wtm_bonus[eg] : -wtm_bonus[eg]);
//  majors =
//      TotalPieces(white, rook) + 2 * TotalPieces(white,
//      queen) - TotalPieces(black, rook) - 2 * TotalPieces(black, queen);
//  minors =
//      TotalPieces(white, knight) + TotalPieces(white,
//      bishop) - TotalPieces(black, knight) - TotalPieces(black, bishop);
//  if (majors || minors) {
//    if (Abs(TotalPieces(white, occupied) - TotalPieces(black, occupied)) != 2
//        && TotalPieces(white, occupied) - TotalPieces(black, occupied) != 0) {
//      score_mg +=
//          Sign(TotalPieces(white, occupied) - TotalPieces(black,
//              occupied)) * bad_trade;
//      score_eg +=
//          Sign(TotalPieces(white, occupied) - TotalPieces(black,
//              occupied)) * bad_trade;
//    }
//  }
//  tree->score_mg += score_mg;
//  tree->score_eg += score_eg;
//}
