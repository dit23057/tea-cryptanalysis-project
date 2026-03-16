#include "analysis.h"
#include "tea.h"
#include <stdio.h>
#include <stdlib.h>

/* parity για 4-bit */
static int parity4(unsigned x) {
    x &= 0xF;
    x ^= x >> 2;
    x ^= x >> 1;
    return (int)(x & 1u);
}

/*
  LAT[a][b] = (#x όπου parity(a&x) == parity(b&S(x))) - 8
*/
void compute_lat(int lat[16][16]) {
    for (int a = 0; a < 16; a++) {
        for (int b = 0; b < 16; b++) {
            int cnt = 0;
            for (int x = 0; x < 16; x++) {
                unsigned sx = (unsigned)(tea_sub((uint16_t)x) & 0xF);
                int lhs = parity4((unsigned)(a & x));
                int rhs = parity4((unsigned)(b & sx));
                if (lhs == rhs) cnt++;
            }
            lat[a][b] = cnt - 8;
        }
    }
}

/*
  DDT[dx][dy] = # { x | S(x) XOR S(x XOR dx) = dy }
*/
void compute_ddt(int ddt[16][16]) {
    for (int dx = 0; dx < 16; dx++) {
        for (int dy = 0; dy < 16; dy++) ddt[dx][dy] = 0;

        for (int x = 0; x < 16; x++) {
            unsigned sx1 = (unsigned)(tea_sub((uint16_t)x) & 0xF);
            unsigned sx2 = (unsigned)(tea_sub((uint16_t)(x ^ dx)) & 0xF);
            int dy = (int)(sx1 ^ sx2);
            ddt[dx][dy]++;
        }
    }
}

void print_lat(const int lat[16][16]) {
    printf("LAT (τιμές = count-8, για 16 τιμές x)\n    ");
    for (int b = 0; b < 16; b++) printf("%3X ", b);
    printf("\n");
    for (int a = 0; a < 16; a++) {
        printf("%3X ", a);
        for (int b = 0; b < 16; b++) printf("%3d ", lat[a][b]);
        printf("\n");
    }
}

void print_ddt(const int ddt[16][16]) {
    printf("DDT (counts out of 16)\n    ");
    for (int dy = 0; dy < 16; dy++) printf("%3X ", dy);
    printf("\n");
    for (int dx = 0; dx < 16; dx++) {
        printf("%3X ", dx);
        for (int dy = 0; dy < 16; dy++) printf("%3d ", ddt[dx][dy]);
        printf("\n");
    }
}

/* -------- Top-10 helpers -------- */
typedef struct {
    int a, b;
    int latv;
} LinEntry;

typedef struct {
    int dx, dy;
    int cnt;
} DiffEntry;

static int cmp_lin(const void* p1, const void* p2) {
    const LinEntry* x = (const LinEntry*)p1;
    const LinEntry* y = (const LinEntry*)p2;
    int ax = abs(x->latv), ay = abs(y->latv);
    return ay - ax;
}

static int cmp_diff(const void* p1, const void* p2) {
    const DiffEntry* x = (const DiffEntry*)p1;
    const DiffEntry* y = (const DiffEntry*)p2;
    return y->cnt - x->cnt;
}

void top10_sbox_linear(const int lat[16][16]) {
    LinEntry list[16*16];
    int idx = 0;

    for (int a = 0; a < 16; a++) {
        for (int b = 0; b < 16; b++) {
            list[idx++] = (LinEntry){a, b, lat[a][b]};
        }
    }

    qsort(list, (size_t)idx, sizeof(list[0]), cmp_lin);

    printf("\nTop-10 S-box γραμμικές προσεγγίσεις (a->b)\n");
    int shown = 0;
    for (int i = 0; i < idx && shown < 10; i++) {
        if (list[i].a == 0 && list[i].b == 0) continue;
        if (list[i].latv == 0) break;

        double bias = list[i].latv / 16.0;
        double p = 0.5 + bias / 2.0;

        printf("%2d) a=%X, b=%X, LAT=%+d, bias=%+.4f, p=%.4f\n",
               shown + 1, list[i].a, list[i].b, list[i].latv, bias, p);
        shown++;
    }
}

void top10_sbox_diff(const int ddt[16][16]) {
    DiffEntry list[16*16];
    int idx = 0;

    for (int dx = 0; dx < 16; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            list[idx++] = (DiffEntry){dx, dy, ddt[dx][dy]};
        }
    }

    qsort(list, (size_t)idx, sizeof(list[0]), cmp_diff);

    printf("\nTop-10 S-box διαφορικά (dx->dy)\n");
    int shown = 0;
    for (int i = 0; i < idx && shown < 10; i++) {
        if (list[i].dx == 0 && list[i].dy == 0) continue;
        if (list[i].cnt == 0) break;

        double p = list[i].cnt / 16.0;
        printf("%2d) dx=%X, dy=%X, count=%d, p=%.4f\n",
               shown + 1, list[i].dx, list[i].dy, list[i].cnt, p);
        shown++;
    }
}
