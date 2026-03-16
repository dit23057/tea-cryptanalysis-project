#ifndef TRAIL_ATTACK_H
#define TRAIL_ATTACK_H

#include <stdint.h>

/* Ένα differential trail 3 γύρων */
typedef struct {
    uint16_t dP;         /* διαφορά plaintext */
    uint16_t dU2;        /* διαφορά στην είσοδο του τελευταίου S-layer (round 2) */
    uint16_t dC;         /* διαφορά ciphertext (μετά το τελευταίο S-layer) */
    double   prob;       /* πιθανότητα trail */
    int      active_last;/* πόσα active nibbles στο ΔU2 */
} Trail3;

/* βρίσκει top-10 trails για 3 γύρους (γρήγορη αναζήτηση) */
void find_top10_trails_3round(const int ddt[16][16], Trail3 out10[10]);

/* differential attack 3 γύρων: μαντεύει 1 nibble του k3 αν το trail έχει active_last=1 */
void differential_attack_3round(const Trail3* tr, uint16_t secretK, int pairs);

#endif
