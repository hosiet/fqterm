#include "ssh_pubkey_crypto.h"
#include <stdlib.h>

struct ssh_pubkey_t*
ssh_pubkey_new(enum pubkey_type t)
{
	struct ssh_pubkey_t *k = (struct ssh_pubkey_t*)
		malloc(sizeof(struct ssh_pubkey_t));
	k->key_type = t;
	switch (t) {
	case SSH_RSA:
		k->key.ssh_rsa = RSA_new();
	}
	return k;
}

int
ssh_pubkey_free(struct ssh_pubkey_t *k)
{
	switch (k->key_type) {
	case SSH_RSA:
		RSA_free(k->key.ssh_rsa);
	}
	free(k);
}

static int
ssh_pubkey_encrypt_rsa(RSA *k, BIGNUM *out, BIGNUM *in)
{
	size_t len, ilen, olen;

	olen = RSA_size(k);
	ilen = BN_num_bytes(in);

	unsigned char outbuf[olen], inbuf[ilen];

	BN_bn2bin(in, inbuf);
	len = RSA_public_encrypt(ilen, inbuf, outbuf, k,
				 RSA_PKCS1_PADDING);
	if (len <= 0) {
		return -1;
	}
	BN_bin2bn(outbuf, len, out);
	return 0;
}

int ssh_pubkey_encrypt(struct ssh_pubkey_t *k, BIGNUM *out, BIGNUM *in)
{
	switch (k->key_type) {
	case SSH_RSA:
		return ssh_pubkey_encrypt_rsa(k->key.ssh_rsa, out, in);
	}
}
