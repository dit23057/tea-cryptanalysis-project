#ifndef ANALYSIS_H
#define ANALYSIS_H

/* LAT / DDT για το S-box (4-bit) */
void compute_lat(int lat[16][16]);
void compute_ddt(int ddt[16][16]);

void print_lat(const int lat[16][16]);
void print_ddt(const int ddt[16][16]);

void top10_sbox_linear(const int lat[16][16]);
void top10_sbox_diff(const int ddt[16][16]);

#endif
