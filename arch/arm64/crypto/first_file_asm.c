#include <linux/init.h>

const int first_crypto_asm_rodata = 10;
int       first_crypto_asm_data   = 20;


void first_crypto_asm_text (void) __attribute__((unused));
void first_crypto_asm_text (void)
{
}

void __init first_crypto_asm_init(void) __attribute__((unused));
void __init first_crypto_asm_init(void)
{
}

void __exit first_crypto_asm_exit(void) __attribute__((unused));
void __exit first_crypto_asm_exit(void)
{
}
