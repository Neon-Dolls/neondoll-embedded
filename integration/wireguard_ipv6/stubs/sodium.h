// Only the public libsodium ABI used by upstream. Linked to the real host library;
// no replacement crypto implementation or test double.
#ifndef ND_SODIUM_ABI_H
#define ND_SODIUM_ABI_H
int crypto_scalarmult_curve25519(unsigned char *, const unsigned char *, const unsigned char *);
#endif
