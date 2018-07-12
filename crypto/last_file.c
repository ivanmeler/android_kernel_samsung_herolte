#include <linux/init.h>

const int last_crypto_rodata = 1000;
int       last_crypto_data   = 2000;

void last_crypto_text(void) __attribute__((unused));
void last_crypto_text (void)
{
}

void __init last_crypto_init(void) __attribute__((unused));
void __init last_crypto_init(void)
{
}

void __exit last_crypto_exit(void) __attribute__((unused));
void __exit last_crypto_exit(void)
{
}
