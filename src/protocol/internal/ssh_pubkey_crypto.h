/* This file is part of FQTerm project
 * written by Iru Cai <mytbk920423@gmail.com>
 */

#ifndef SSH_PUBKEY_CRYPTO_H
#define SSH_PUBKEY_CRYPTO_H

#include <openssl/rsa.h>

#ifdef __cplusplus
extern "C" {
#endif

	enum pubkey_type {
		SSH_RSA
	};

	struct ssh_pubkey_t
	{
		enum pubkey_type key_type; /* now only RSA is supported */
		union {
			RSA *ssh_rsa;
		} key;
	};

	struct ssh_pubkey_t *ssh_pubkey_new(enum pubkey_type);
	int ssh_pubkey_free(struct ssh_pubkey_t*);
	int ssh_pubkey_encrypt(struct ssh_pubkey_t *k, BIGNUM *out, BIGNUM *in);

#ifdef __cplusplus
}
#endif

#endif
