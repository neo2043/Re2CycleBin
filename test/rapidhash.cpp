#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rapidhash.h"

static void print_hash(const char *label, uint64_t hash) {
    printf("%-18s : 0x%016llx\n", label, (unsigned long long)hash);
}

int main(void) {
    const char *input = "RapidHash Integration Test";
    size_t len = strlen(input);

    uint64_t h1 = rapidhash(input, len);
    uint64_t h2 = rapidhashMicro(input, len);
    uint64_t h3 = rapidhashNano(input, len);

    printf("=== RapidHash Verification ===\n");
    printf("Input string: \"%s\"\n", input);
    print_hash("rapidhash", h1);
    print_hash("rapidhashMicro", h2);
    print_hash("rapidhashNano", h3);

    // Simple sanity check: the three should differ but be non-zero
    if (h1 && h2 && h3 && (h1 != h2) && (h2 != h3)) {
        printf("\n✅ RapidHash successfully included and functioning.\n");
        return 0;
    } else {
        printf("\n❌ Something is wrong: possible inclusion or compilation issue.\n");
        return 1;
    }
}
