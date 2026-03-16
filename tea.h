#ifndef TEA_TOY_H
#define TEA_TOY_H

#include <stdint.h>

/*
  Toy TEA (SPN) από την εκφώνηση:
  - block: 16 bit
  - key:   16 bit
  - 4 γύροι + τελικό XOR με k4
*/
#define TEA_ROUNDS 4

/* Κρυπτογράφηση / Αποκρυπτογράφηση πλήρους TEA (4 γύροι) */
uint16_t tea_encrypt(uint16_t M, uint16_t K);
uint16_t tea_decrypt(uint16_t C, uint16_t K);

/* Reduced cipher: κρυπτογράφηση/αποκρυπτογράφηση για r γύρους (για attack 3 γύρων) */
uint16_t tea_encrypt_r(uint16_t M, uint16_t K, unsigned r);
uint16_t tea_decrypt_r(uint16_t C, uint16_t K, unsigned r);

/* Παραγωγή υποκλειδιών k0..k4 */
void tea_key_schedule(uint16_t K, uint16_t rk[TEA_ROUNDS + 1]);

/* Δομικά στοιχεία */
uint16_t tea_sub(uint16_t x);
uint16_t tea_inv_sub(uint16_t x);

uint16_t tea_perm(uint16_t x);
uint16_t tea_inv_perm(uint16_t x);

#endif
