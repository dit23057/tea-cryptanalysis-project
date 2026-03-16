#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "tea.h"
#include "analysis.h"
#include "trail_attack.h"

/* μικρό RNG */
static uint32_t xorshift32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* 0) self-test */
static void self_test(void) {
    uint32_t seed = 0xCAFEBABEu;
    for (int i = 0; i < 20000; i++) {
        uint16_t M = (uint16_t)xorshift32(&seed);
        uint16_t K = (uint16_t)xorshift32(&seed);
        uint16_t C = tea_encrypt(M, K);
        uint16_t D = tea_decrypt(C, K);
        if (D != M) {
            printf("SELF-TEST FAIL: M=%04X K=%04X C=%04X D=%04X\n",
                   (unsigned)M, (unsigned)K, (unsigned)C, (unsigned)D);
            return;
        }
    }
    printf("SELF-TEST OK (20000 τυχαία ζεύγη)\n");
}

/* 1) vectors για K=0 */
static void mode_vectors_K0(void) {
    FILE* f = fopen("vectors_K0.txt", "w");
    if (!f) { perror("fopen"); return; }

    uint16_t K = 0x0000;
    for (uint32_t M = 0; M <= 0xFFFFu; M++) {
        uint16_t C = tea_encrypt((uint16_t)M, K);
        fprintf(f, "%04X %04X\n", (unsigned)M, (unsigned)C);
    }
    fclose(f);
    printf("Έτοιμο: vectors_K0.txt (M,C για K=0000)\n");
}

/* 2) vectors για M=0 */
static void mode_vectors_M0(void) {
    FILE* f = fopen("vectors_M0.txt", "w");
    if (!f) { perror("fopen"); return; }

    uint16_t M = 0x0000;
    for (uint32_t K = 0; K <= 0xFFFFu; K++) {
        uint16_t C = tea_encrypt(M, (uint16_t)K);
        fprintf(f, "%04X %04X\n", (unsigned)K, (unsigned)C);
    }
    fclose(f);
    printf("Έτοιμο: vectors_M0.txt (K,C για M=0000)\n");
}

/* 3) υποκλειδιά για K=A1E9 */
static void mode_subkeys_a1e9(void) {
    uint16_t rk[TEA_ROUNDS + 1];
    uint16_t K = 0xA1E9;

    tea_key_schedule(K, rk);

    printf("K = %04X\n", (unsigned)K);
    for (int i = 0; i <= TEA_ROUNDS; i++) {
        printf("k%d = %04X\n", i, (unsigned)rk[i]);
    }
    printf("\nΣτον πίνακα της εκφώνησης συμπληρώνω k1..k4.\n");
}

/* 4) benchmark ~1GiB */
static void mode_benchmark_1GiB(void) {
    const uint32_t N = 1u << 26; /* 67,108,864 blocks */
    uint16_t* buf = (uint16_t*)malloc((size_t)N * sizeof(uint16_t));

    if (!buf) {
        printf("malloc failed: χρειάζομαι ~128MiB RAM.\n");
        return;
    }

    uint32_t seed = 0x12345678u;
    for (uint32_t i = 0; i < N; i++) {
        buf[i] = (uint16_t)xorshift32(&seed);
    }

    uint16_t K = 0xA1E9;

    clock_t t0 = clock();
    for (uint32_t i = 0; i < N; i++) buf[i] = tea_encrypt(buf[i], K);
    clock_t t1 = clock();
    for (uint32_t i = 0; i < N; i++) buf[i] = tea_decrypt(buf[i], K);
    clock_t t2 = clock();

    double enc_sec = (double)(t1 - t0) / CLOCKS_PER_SEC;
    double dec_sec = (double)(t2 - t1) / CLOCKS_PER_SEC;

    printf("Encryption: %.3f sec\n", enc_sec);
    printf("Decryption: %.3f sec\n", dec_sec);
    printf("(Κάνε screenshot αυτά τα 2 νούμερα)\n");

    free(buf);
}

/* 5) LAT/DDT + top-10 S-box */
static void mode_lat_ddt_top10(void) {
    int lat[16][16];
    int ddt[16][16];

    compute_lat(lat);
    compute_ddt(ddt);

    print_lat(lat);
    print_ddt(ddt);

    top10_sbox_linear(lat);
    top10_sbox_diff(ddt);

    printf("\nΣημείωση: αυτά είναι για *1 S-box*.\n");
    printf("Για 3 γύρους (2β) θα χρησιμοποιήσω trails (mode 6).\n");
}

/* 6) top-10 trails 3 γύρων */
static void mode_top10_trails_3round(void) {
    int ddt[16][16];
    compute_ddt(ddt);

    Trail3 top10[10];
    find_top10_trails_3round(ddt, top10);

    printf("\nTop-10 differential trails (3 rounds)\n");
    for (int i = 0; i < 10; i++) {
        printf("%2d) dP=%04X  dU2=%04X  dC=%04X  prob≈%.8f  active_last=%d\n",
               i+1, top10[i].dP, top10[i].dU2, top10[i].dC, top10[i].prob, top10[i].active_last);
    }
    printf("\nTip: Για attack προτιμάμε trail με active_last=1.\n");
}

/* 7) differential attack 3 γύρων */
static void mode_attack_3round(void) {
    int ddt[16][16];
    compute_ddt(ddt);

    Trail3 top10[10];
    find_top10_trails_3round(ddt, top10);

    /* διάλεξε πρώτο trail με active_last=1, αλλιώς πάρε το #1 */
    Trail3 chosen = top10[0];
    for (int i = 0; i < 10; i++) {
        if (top10[i].active_last == 1) { chosen = top10[i]; break; }
    }

    uint16_t secretK = 0xA1E9; /* για δοκιμή */
    int pairs = 200000;        /* αν αργεί: 50000 */

    differential_attack_3round(&chosen, secretK, pairs);
}

int main(void) {
#ifdef _WIN32
    /* για να εμφανίζονται σωστά τα ελληνικά */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    printf("\n--- TEA toy SPN (εργασία) ---\n");
    printf("0) Self-test\n");
    printf("1) Test vectors (K=0000)\n");
    printf("2) Test vectors (M=0000)\n");
    printf("3) Υποκλειδιά για K=A1E9\n");
    printf("4) Benchmark ~1GiB (2^26 blocks)\n");
    printf("5) LAT/DDT + top-10 (S-box)\n");
    printf("6) Top-10 differential trails (3 rounds)\n");
    printf("7) Differential attack (3 rounds)\n");
    printf("Επιλογή: ");

    int mode = -1;
    if (scanf("%d", &mode) != 1) return 0;

    switch (mode) {
        case 0: self_test(); break;
        case 1: mode_vectors_K0(); break;
        case 2: mode_vectors_M0(); break;
        case 3: mode_subkeys_a1e9(); break;
        case 4: mode_benchmark_1GiB(); break;
        case 5: mode_lat_ddt_top10(); break;
        case 6: mode_top10_trails_3round(); break;
        case 7: mode_attack_3round(); break;
        default: printf("Άγνωστη επιλογή.\n"); break;
    }

    return 0;
}
