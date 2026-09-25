#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *JWT_SECRET = "realworld_secret_key";

void hash_password(const char *password, char *out_hash) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)password, strlen(password), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out_hash + (i * 2), "%02x", hash[i]);
    }
    out_hash[64] = '\0';
}

static void base64url_encode(const unsigned char *input, int length, char *output) {
    int encoded_len = EVP_EncodeBlock((unsigned char*)output, input, length);
    // Convert to base64url
    for (int i = 0; i < encoded_len; i++) {
        if (output[i] == '+') output[i] = '-';
        else if (output[i] == '/') output[i] = '_';
        else if (output[i] == '=') {
            output[i] = '\0';
            break;
        }
    }
}

static int base64url_decode(const char *input, unsigned char *output) {
    int len = strlen(input);
    char *temp = malloc(len + 4);
    strcpy(temp, input);
    for (int i = 0; i < len; i++) {
        if (temp[i] == '-') temp[i] = '+';
        else if (temp[i] == '_') temp[i] = '/';
    }
    while (strlen(temp) % 4 != 0) {
        strcat(temp, "=");
    }
    int out_len = EVP_DecodeBlock(output, (const unsigned char*)temp, strlen(temp));
    free(temp);
    return out_len;
}

char* jwt_generate(int user_id, const char *username) {
    const char *header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char payload[256];
    snprintf(payload, sizeof(payload), "{\"id\":%d,\"username\":\"%s\"}", user_id, username);

    char b64_header[256] = {0};
    char b64_payload[512] = {0};
    base64url_encode((const unsigned char*)header, strlen(header), b64_header);
    base64url_encode((const unsigned char*)payload, strlen(payload), b64_payload);

    char message[1024];
    snprintf(message, sizeof(message), "%s.%s", b64_header, b64_payload);

    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int mac_len = 0;
    HMAC(EVP_sha256(), JWT_SECRET, strlen(JWT_SECRET), (const unsigned char*)message, strlen(message), mac, &mac_len);

    char b64_sig[256] = {0};
    base64url_encode(mac, mac_len, b64_sig);

    char *token = malloc(2048);
    snprintf(token, 2048, "%s.%s.%s", b64_header, b64_payload, b64_sig);
    return token;
}

int jwt_verify(const char *token, int *out_user_id) {
    char *t = strdup(token);
    char *header_part = strtok(t, ".");
    char *payload_part = strtok(NULL, ".");
    char *sig_part = strtok(NULL, ".");

    if (!header_part || !payload_part || !sig_part) {
        free(t);
        return 0;
    }

    char message[1024];
    snprintf(message, sizeof(message), "%s.%s", header_part, payload_part);

    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int mac_len = 0;
    HMAC(EVP_sha256(), JWT_SECRET, strlen(JWT_SECRET), (const unsigned char*)message, strlen(message), mac, &mac_len);

    char b64_expected_sig[256] = {0};
    base64url_encode(mac, mac_len, b64_expected_sig);

    if (strcmp(sig_part, b64_expected_sig) != 0) {
        free(t);
        return 0;
    }

    unsigned char decoded_payload[512] = {0};
    base64url_decode(payload_part, decoded_payload);
    
    char *id_str = strstr((char*)decoded_payload, "\"id\":");
    if (id_str) {
        *out_user_id = atoi(id_str + 5);
        free(t);
        return 1;
    }

    free(t);
    return 0;
}
