/* Transcript cross-check: run a fixed sequence of absorbs/challenges,
 * print the challenges. Compare against Python transcript. */
#include <stdio.h>
#include <stdint.h>
#include "field.h"
#include "transcript.h"

int main(void) {
    transcript_t t;
    transcript_init(&t, "plonk-fri", 9);

    uint8_t root[32];
    for (int i = 0; i < 32; ++i) root[i] = (uint8_t)(i * 7 + 1);
    transcript_absorb_root(&t, "commit_a", root);

    felt beta = transcript_challenge_field(&t, "beta");
    felt gamma = transcript_challenge_field(&t, "gamma");

    felt elems[3] = { f_from_u64(11), f_from_u64(22), f_from_u64(33) };
    transcript_absorb_elems(&t, "openings", elems, 3);

    felt zeta = transcript_challenge_field(&t, "zeta");
    uint32_t q = transcript_challenge_int(&t, "fri_query", 64);

    printf("field=%s\n", FIELD_NAME);
    printf("beta=%llu\n",  (unsigned long long)f_to_u64(beta));
    printf("gamma=%llu\n", (unsigned long long)f_to_u64(gamma));
    printf("zeta=%llu\n",  (unsigned long long)f_to_u64(zeta));
    printf("q=%u\n", q);
    return 0;
}
