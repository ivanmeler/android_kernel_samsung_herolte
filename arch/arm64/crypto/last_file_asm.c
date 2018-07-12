#include <linux/init.h>

const int last_crypto_asm_rodata = 1000;
int       last_crypto_asm_data   = 2000;

void last_crypto_asm_text(void) __attribute__((unused));
void last_crypto_asm_text (void)
{
}

void __init last_crypto_asm_init(void) __attribute__((unused));
void __init last_crypto_asm_init(void)
{
}

void __exit last_crypto_asm_exit(void) __attribute__((unused));
void __exit last_crypto_asm_exit(void)
{
}
