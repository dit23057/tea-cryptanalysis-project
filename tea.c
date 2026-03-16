#include "tea.h"
#include <stdint.h>

/*
  Υλοποίηση toy TEA SPN.

  S-box 4x4:
  S(x) = 6 4 c 5 0 7 2 e 1 f 3 d 8 a 9 b

  Γύροι (4):
    x ^= k[r]
    x = S-layer
    x = P-layer (εκτός τελευταίου γύρου)
  Τέλος:
    x ^= k4
*/

static const uint8_t S[16] = {
    0x6,0x4,0xC,0x5,0x0,0x7,0x2,0xE,0x1,0xF,0x3,0xD,0x8,0xA,0x9,0xB
};

/* inverse S */
static const uint8_t Si[16] = {
    0x4,0x8,0x6,0xA,0x1,0x3,0x0,0x5,0xC,0xE,0xD,0xF,0x2,0xB,0x7,0x9
};

static inline uint8_t rol8(uint8_t x, unsigned r) {
    r &= 7u;
    return (uint8_t)((x << r) | (x >> (8u - r)));
}

static inline uint16_t get_bit(uint16_t x, unsigned i) {
    return (uint16_t)((x >> i) & 1u);
}
static inline uint16_t set_bit(uint16_t x, unsigned i, uint16_t b) {
    x &= (uint16_t)~(1u << i);
    x |= (uint16_t)((b & 1u) << i);
    return x;
}

/*
  Permutation 16-bit (transpose 4x4):
  P(i) = (i%4)*4 + (i/4)
*/
static inline unsigned P(unsigned i) {
    return (unsigned)((i % 4u) * 4u + (i / 4u));
}

uint16_t tea_perm(uint16_t x) {
    uint16_t y = 0;
    for (unsigned i = 0; i < 16; i++) {
        y = set_bit(y, P(i), get_bit(x, i));
    }
    return y;
}

uint16_t tea_inv_perm(uint16_t x) {
    uint16_t y = 0;
    for (unsigned j = 0; j < 16; j++) {
        y = set_bit(y, j, get_bit(x, P(j)));
    }
    return y;
}

uint16_t tea_sub(uint16_t x) {
    uint16_t y = 0;
    for (unsigned n = 0; n < 4; n++) {
        uint8_t nib = (uint8_t)((x >> (4u*n)) & 0xFu);
        y |= (uint16_t)(S[nib] << (4u*n));
    }
    return y;
}

uint16_t tea_inv_sub(uint16_t x) {
    uint16_t y = 0;
    for (unsigned n = 0; n < 4; n++) {
        uint8_t nib = (uint8_t)((x >> (4u*n)) & 0xFu);
        y |= (uint16_t)(Si[nib] << (4u*n));
    }
    return y;
}

/*
  Key schedule:
  - κυκλική αριστερή ολίσθηση 2 (στο κάθε byte)
  - μετά αντιμετάθεση (perm) εκτός του τελευταίου υποκλειδιού (k4)
*/
void tea_key_schedule(uint16_t K, uint16_t rk[TEA_ROUNDS + 1]) {
    rk[0] = K;

    uint16_t cur = K;
    for (unsigned i = 1; i <= TEA_ROUNDS; i++) {
        uint8_t hi = (uint8_t)(cur >> 8);
        uint8_t lo = (uint8_t)(cur & 0xFFu);

        hi = rol8(hi, 2);
        lo = rol8(lo, 2);

        cur = (uint16_t)((hi << 8) | lo);

        if (i < TEA_ROUNDS) {
            cur = tea_perm(cur);
        }
        rk[i] = cur;
    }
}

uint16_t tea_encrypt(uint16_t M, uint16_t K) {
    uint16_t rk[TEA_ROUNDS + 1];
    tea_key_schedule(K, rk);

    uint16_t x = M;

    for (unsigned r = 0; r < TEA_ROUNDS; r++) {
        x ^= rk[r];
        x = tea_sub(x);
        if (r < TEA_ROUNDS - 1) x = tea_perm(x);
    }

    x ^= rk[TEA_ROUNDS];
    return x;
}

uint16_t tea_decrypt(uint16_t C, uint16_t K) {
    uint16_t rk[TEA_ROUNDS + 1];
    tea_key_schedule(K, rk);

    uint16_t x = C;
    x ^= rk[TEA_ROUNDS];

    for (int r = TEA_ROUNDS - 1; r >= 0; r--) {
        if (r < (int)TEA_ROUNDS - 1) x = tea_inv_perm(x);
        x = tea_inv_sub(x);
        x ^= rk[(unsigned)r];
    }
    return x;
}

/*
  Reduced έκδοση για r γύρους (χρήσιμη για 3-round attack).
  Για r γύρους χρησιμοποιώ k0..k[r] (τελικό XOR με k[r]).
*/
uint16_t tea_encrypt_r(uint16_t M, uint16_t K, unsigned r) {
    if (r == 0 || r > TEA_ROUNDS) r = TEA_ROUNDS;

    uint16_t rk[TEA_ROUNDS + 1];
    tea_key_schedule(K, rk);

    uint16_t x = M;
    for (unsigned round = 0; round < r; round++) {
        x ^= rk[round];
        x = tea_sub(x);
        if (round < r - 1) x = tea_perm(x);
    }
    x ^= rk[r];
    return x;
}

uint16_t tea_decrypt_r(uint16_t C, uint16_t K, unsigned r) {
    if (r == 0 || r > TEA_ROUNDS) r = TEA_ROUNDS;

    uint16_t rk[TEA_ROUNDS + 1];
    tea_key_schedule(K, rk);

    uint16_t x = C;
    x ^= rk[r];

    for (int round = (int)r - 1; round >= 0; round--) {
        if (round < (int)r - 1) x = tea_inv_perm(x);
        x = tea_inv_sub(x);
        x ^= rk[(unsigned)round];
    }
    return x;
}
