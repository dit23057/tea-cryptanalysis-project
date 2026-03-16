#include "trail_attack.h"
#include "tea.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* --------- βοηθητικά --------- */

typedef struct { uint8_t dy; uint8_t cnt; } DTrans;

static void build_transitions(const int ddt[16][16], DTrans trans[16][16], int tcount[16]) {
    for (int dx = 0; dx < 16; dx++) {
        int k = 0;
        for (int dy = 0; dy < 16; dy++) {
            if (ddt[dx][dy] > 0) {
                trans[dx][k].dy = (uint8_t)dy;
                trans[dx][k].cnt = (uint8_t)ddt[dx][dy];
                k++;
            }
        }
        tcount[dx] = k;
    }
}

static int active_nibbles(uint16_t x) {
    int c = 0;
    for (int i = 0; i < 4; i++) {
        if (((x >> (4*i)) & 0xF) != 0) c++;
    }
    return c;
}

typedef struct {
    uint16_t d_in;
    uint16_t d_out;
    double   prob;
} Node;

static int cmp_node_prob_desc(const void* a, const void* b) {
    const Node* x = (const Node*)a;
    const Node* y = (const Node*)b;
    if (y->prob > x->prob) return 1;
    if (y->prob < x->prob) return -1;
    return 0;
}

/*
  Expand ενός S-layer σε differential:
  - για κάθε nibble, με βάση DDT, παίρνω πιθανά dy και τις πιθανότητες (cnt/16)
  - για να μην εκραγεί ο αριθμός, κρατάω τα καλύτερα B (beam)
*/
static int expand_slayer_beam(
    uint16_t din,
    double baseProb,
    const DTrans trans[16][16],
    const int tcount[16],
    Node* out,
    int outCap,
    int B
){
    DTrans opts[4][16];
    int oc[4];

    for (int i = 0; i < 4; i++) {
        int dx = (din >> (4*i)) & 0xF;
        oc[i] = tcount[dx];
        for (int j = 0; j < oc[i]; j++) opts[i][j] = trans[dx][j];
    }

    Node tmp1[10000], tmp2[10000];
    int n1 = 1;
    tmp1[0] = (Node){din, 0, baseProb};

    for (int nib = 0; nib < 4; nib++) {
        int n2 = 0;
        for (int i = 0; i < n1; i++) {
            for (int j = 0; j < oc[nib]; j++) {
                uint16_t dout = tmp1[i].d_out;
                double p = tmp1[i].prob;

                uint8_t dy = opts[nib][j].dy;
                uint8_t cnt = opts[nib][j].cnt;

                int dx = (din >> (4*nib)) & 0xF;
                if (dx != 0) p *= ((double)cnt / 16.0);

                dout |= (uint16_t)(dy << (4*nib));

                if (n2 < (int)(sizeof(tmp2)/sizeof(tmp2[0]))) {
                    tmp2[n2++] = (Node){din, dout, p};
                }
            }
        }

        qsort(tmp2, (size_t)n2, sizeof(tmp2[0]), cmp_node_prob_desc);
        if (n2 > B) n2 = B;

        for (int i = 0; i < n2; i++) tmp1[i] = tmp2[i];
        n1 = n2;
    }

    if (n1 > outCap) n1 = outCap;
    for (int i = 0; i < n1; i++) out[i] = tmp1[i];
    return n1;
}

/* comparator για trails */
static int cmp_trail3(const void* p1, const void* p2) {
    const Trail3* a = (const Trail3*)p1;
    const Trail3* b = (const Trail3*)p2;

    if (b->prob > a->prob) return 1;
    if (b->prob < a->prob) return -1;

    return a->active_last - b->active_last;
}

/*
  Αναζήτηση trails για 3 γύρους:
    Round0: S -> P
    Round1: S -> P
    Round2: S (χωρίς P)
  Δεν είναι εξαντλητικό 100%, είναι “φοιτητικό” beam ώστε να βγαίνουν καλά trails γρήγορα.
*/
void find_top10_trails_3round(const int ddt[16][16], Trail3 out10[10]) {
    DTrans trans[16][16];
    int tcount[16];
    build_transitions(ddt, trans, tcount);

    const int B = 800; /* beam size */

    Trail3 pool[3000];
    int poolN = 0;

    /* δοκιμάζω ΔP με 1-2 active nibbles */
    for (uint32_t dP = 1; dP <= 0xFFFFu; dP++) {
        if (active_nibbles((uint16_t)dP) > 2) continue;

        /* Round0 S */
        Node r0S[2000];
        int n0 = expand_slayer_beam((uint16_t)dP, 1.0, trans, tcount, r0S, 2000, B);

        for (int i0 = 0; i0 < n0; i0++) {
            uint16_t dAfterP0 = tea_perm(r0S[i0].d_out);

            /* Round1 S */
            Node r1S[2000];
            int n1 = expand_slayer_beam(dAfterP0, r0S[i0].prob, trans, tcount, r1S, 2000, B);

            for (int i1 = 0; i1 < n1; i1++) {
                uint16_t dAfterP1 = tea_perm(r1S[i1].d_out);

                /* Round2 S */
                Node r2S[2000];
                int n2 = expand_slayer_beam(dAfterP1, r1S[i1].prob, trans, tcount, r2S, 2000, B);

                for (int i2 = 0; i2 < n2; i2++) {
                    Trail3 tr;
                    tr.dP = (uint16_t)dP;
                    tr.dU2 = dAfterP1;      /* είσοδος τελευταίου S-layer */
                    tr.dC  = r2S[i2].d_out; /* έξοδος τελευταίου S-layer (τελικό XOR δεν αλλάζει διαφορά) */
                    tr.prob = r2S[i2].prob;
                    tr.active_last = active_nibbles(tr.dU2);

                    if (poolN < (int)(sizeof(pool)/sizeof(pool[0]))) {
                        pool[poolN++] = tr;
                    }
                }
            }
        }

        /* stop νωρίς για να μην αργεί τρελά */
        if (dP > 0x0FFFu && poolN > 2000) break;
    }

    qsort(pool, (size_t)poolN, sizeof(pool[0]), cmp_trail3);

    for (int i = 0; i < 10; i++) out10[i] = pool[i];
}

/* ---------- Attack helpers ---------- */

static uint8_t Si_nibble(uint8_t y) {
    static const uint8_t Si_local[16] = {0x4,0x8,0x6,0xA,0x1,0x3,0x0,0x5,0xC,0xE,0xD,0xF,0x2,0xB,0x7,0x9};
    return Si_local[y & 0xF];
}

static int pick_single_active_nibble(uint16_t dU2) {
    int pos = -1;
    for (int i = 0; i < 4; i++) {
        if (((dU2 >> (4*i)) & 0xF) != 0) {
            if (pos != -1) return -1;
            pos = i;
        }
    }
    return pos;
}

/*
  Differential attack (3 rounds):
  - χρειάζεται trail που στο ΔU2 έχει 1 ενεργό nibble (active_last=1)
  - παίρνω πολλά ζεύγη (P, P^ΔP)
  - φιλτράρω με βάση ΔC
  - κάνω guess το nibble του k3 που αντιστοιχεί στο ενεργό nibble
*/
void differential_attack_3round(const Trail3* tr, uint16_t secretK, int pairs) {
    int nib = pick_single_active_nibble(tr->dU2);
    if (nib == -1) {
        printf("\n[Attack] Το trail δεν έχει 1 active nibble (dU2=%04X).\n", tr->dU2);
        printf("[Attack] Διάλεξε trail με active_last=1 από το mode 6.\n");
        return;
    }

    uint16_t rk[TEA_ROUNDS + 1];
    tea_key_schedule(secretK, rk);
    uint16_t real_k3 = rk[3];
    uint8_t real_k3_n = (uint8_t)((real_k3 >> (4*nib)) & 0xF);

    int score[16] = {0};

    uint32_t seed = 0x12345678u;

    for (int i = 0; i < pairs; i++) {
        seed = seed * 1103515245u + 12345u;
        uint16_t P = (uint16_t)seed;
        uint16_t P2 = (uint16_t)(P ^ tr->dP);

        uint16_t C  = tea_encrypt_r(P,  secretK, 3);
        uint16_t C2 = tea_encrypt_r(P2, secretK, 3);

        uint16_t dC = (uint16_t)(C ^ C2);
        if (dC != tr->dC) continue; /* right pair filter */

        uint8_t cN  = (uint8_t)((C  >> (4*nib)) & 0xF);
        uint8_t c2N = (uint8_t)((C2 >> (4*nib)) & 0xF);

        uint8_t expected_dU = (uint8_t)((tr->dU2 >> (4*nib)) & 0xF);

        for (int g = 0; g < 16; g++) {
            uint8_t u  = Si_nibble((uint8_t)(cN  ^ g));
            uint8_t u2 = Si_nibble((uint8_t)(c2N ^ g));
            uint8_t dU = (uint8_t)(u ^ u2);
            if (dU == expected_dU) score[g]++;
        }
    }

    int bestG = 0;
    for (int g = 1; g < 16; g++) if (score[g] > score[bestG]) bestG = g;

    printf("\n[Attack 3-round] Trail:\n");
    printf("  dP  = %04X\n", tr->dP);
    printf("  dU2 = %04X (active nibble=%d)\n", tr->dU2, nib);
    printf("  dC  = %04X\n", tr->dC);
    printf("  prob≈ %.8f\n", tr->prob);

    printf("\n[Attack 3-round] Αποτέλεσμα guess για nibble του k3:\n");
    printf("  best guess = %X (score=%d)\n", bestG, score[bestG]);
    printf("  real k3    = %04X -> real nibble=%X\n", real_k3, real_k3_n);
    printf("  Success? %s\n", (bestG == real_k3_n) ? "YES" : "NO");
}
