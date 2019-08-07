#include <string.h>

#include <map>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/md5.h>

#include "pdf_internal.h"
#include "pdf_extractor.h"


using namespace std;

const unsigned char padding[] =
"\x28\xBF\x4E\x5E\x4E\x75\x8A\x41\x64\x00\x4E\x56\xFF\xFA\x01\x08\x2E\x2E\x00\xB6\xD0\x68\x3E\x80\x2F\x0C\xA9\xFE\x64\x53\x69\x7A";


typedef enum {
    ENCRYPT_ALGORITHM_RC4V1 = 1, ///< RC4 Version 1 encryption using a 40bit key
    ENCRYPT_ALGORITHM_RC4V2 = 2, ///< RC4 Version 2 encryption using a key with 40-128bit
    ENCRYPT_ALGORITHM_AESV2 = 4,  ///< AES encryption with a 128 bit key (PDF1.6)
    ENCRYPT_ALGORITHM_AESV3 = 8 ///< AES encryption with a 256 bit key (PDF1.7 extension 3)
} encrypt_algorithm_t;

typedef enum {
    PDF_KEY_LENGTH_128 = 128,
    PDF_KEY_LENGTH_256 = 256
} pdf_key_length_t;


/*void base_decrypt(const unsigned char* key,
                  int key_len,
                  const unsigned char* iv,
                  const unsigned char* text_in,
                  int text_len,
                  unsigned char* text_out,
                  int &out_len)
{
    EVP_CIPHER_CTX aes;
    EVP_CIPHER_CTX_init(&aes);
    unique_ptr<EVP_CIPHER_CTX, int (*)(EVP_CIPHER_CTX*)>  aes_scope(&aes, EVP_CIPHER_CTX_cleanup);
	if ((text_len % 16) != 0) throw pdf_error(FUNC_STRING + "error AES-decryption data length not a multiple of 16" );

    int status;
    if (key_len == PDF_KEY_LENGTH_128 / 8) status = EVP_DecryptInit_ex(&aes, EVP_aes_128_cbc(), NULL, key, iv);
    else if (key_len == PDF_KEY_LENGTH_256 / 8) status = EVP_DecryptInit_ex(&aes, EVP_aes_256_cbc(), NULL, key, iv);
    else throw pdf_error(FUNC_STRING + "invalid AES key length");
    if(status != 1) throw pdf_error(FUNC_STRING + "error initializing AES decryption engine" );
    int data_out_moved;
    status = EVP_DecryptUpdate(&aes, text_out, &data_out_moved, text_in, text_len);
	out_len = data_out_moved;
    if(status != 1) throw pdf_error(FUNC_STRING + "Error AES-decryption data" );

    status = EVP_DecryptFinal_ex(&aes, text_out + out_len, &data_out_moved);
	out_len += data_out_moved;
    if(status != 1) throw pdf_error(FUNC_STRING + "Error AES-decryption data final" );
    }*/

array<unsigned char, 32> get_user_pad(const string& password)
{
    array<unsigned char, 32> result;
    size_t i = 0;
    for (; i < result.size() && i < password.size(); ++i) result[i] = static_cast<unsigned char>(password.at(i));
    for (size_t j = 0; i < result.size(); ++i, ++j) result[i] = padding[j];

    return result;
}

bool is_encrypt_metadata(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    auto it = decrypt_opts.find("/EncryptMetadata");
    if (it == decrypt_opts.end()) return true;
    if (it->second.first == "false") return false;
    if (it->second.first == "true") return true;

    throw pdf_error(FUNC_STRING + "wrong bool value:" + it->second.first);
}

int RC4(const unsigned char* key, int key_len, const unsigned char* text_in, int text_len, unsigned char* text_out)

{
    EVP_CIPHER_CTX rc4;
    EVP_CIPHER_CTX_init(&rc4);
    unique_ptr<EVP_CIPHER_CTX, int (*)(EVP_CIPHER_CTX*)>  rc4_scope(&rc4, EVP_CIPHER_CTX_cleanup);
    // Don't set the key because we will modify the parameters
    int status = EVP_EncryptInit_ex(&rc4, EVP_rc4(), NULL, NULL, NULL);
    if(status != 1) throw pdf_error(FUNC_STRING + "RC4 EVP_DecryptInit_ex error");

    status = EVP_CIPHER_CTX_set_key_length(&rc4, key_len);
    if(status != 1) throw pdf_error(FUNC_STRING + "RC4 EVP_CIPHER_CTX_set_key_length error");

    // We finished modifying parameters so now we can set the key
    status = EVP_EncryptInit_ex(&rc4, NULL, NULL, key, NULL);
    if(status != 1)throw pdf_error(FUNC_STRING + "RC4 EVP_DecryptInit_ex error");

    int data_out_moved;
    status = EVP_EncryptUpdate(&rc4, text_out, &data_out_moved, text_in, text_len);
    if(status != 1) throw pdf_error(FUNC_STRING + "RC4 EVP_DecryptUpdate error");
    int written = data_out_moved;
    status = EVP_EncryptFinal_ex(&rc4, &text_out[data_out_moved], &data_out_moved);
    if(status != 1) throw pdf_error(FUNC_STRING + "RC4 EVP_EncryptFinal_ex error" );
    written += data_out_moved;

    return written;
}

string hex_decode(const string &hex)
{
    std::string result;
    for (size_t i = 0; i < hex.length(); i += 2)
    {
        long int d = strtol(hex.substr(i, 2).c_str(), nullptr, 16);
        if (errno != 0) throw pdf_error(FUNC_STRING + "wrong input" + hex);
        result.push_back(static_cast<char>(d));
    }

    return result;
}

string get_string(const string& src)
{
    size_t offset = src.find_first_of("(<", 0);
    if (offset == string::npos) throw pdf_error(FUNC_STRING + "wrong string=" + src);
    char end_delimiter = src[offset] == '('? ')' : '>';
    size_t end_offset = src.find(end_delimiter, offset);
    if (end_offset == string::npos) throw pdf_error(FUNC_STRING + "can`t find end_delimiter=" + end_delimiter);
    ++offset;
    string result = src.substr(offset, end_offset - offset);
    if (end_delimiter == ')') return result;

    return hex_decode(result);
}

vector<unsigned char> string2array(const string &src)
{
    vector<unsigned char> result(src.length());
    for (size_t j = 0; j < src.length(); j++) result[j] = static_cast<unsigned char>(src[j]);

    return result;
}

unsigned int get_length(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    auto it = decrypt_opts.find("/Length");
    if (it == decrypt_opts.end()) return 40 / 8;

    return strict_stoul(it->second.first) / 8;
}

array<unsigned char, 4> get_ext(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    array<unsigned char, 4> ext;
    int p_value = strict_stol(decrypt_opts.at("/P").first);
    ext[0] = static_cast<unsigned char>(p_value & 0xff);
    ext[1] = static_cast<unsigned char>((p_value >>  8) & 0xff);
    ext[2] = static_cast<unsigned char>((p_value >> 16) & 0xff);
    ext[3] = static_cast<unsigned char>((p_value >> 24) & 0xff);

    return ext;
}

void md5_init_exc(MD5_CTX *ctx)
{
    if (MD5_Init(ctx) != 1) throw pdf_error(FUNC_STRING + "Error initializing MD5 hashing engine" );
}

void md5_update_exc(MD5_CTX *c, const void *data, unsigned long len)
{
    if (MD5_Update(c, data, len) != 1) throw pdf_error(FUNC_STRING + "Error MD5 update" );
}

vector<unsigned char> get_encryption_key(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    unsigned int key_length = get_length(decrypt_opts);

    MD5_CTX ctx;
    md5_init_exc(&ctx);
    md5_update_exc(&ctx, padding, 32);
    const string o_val = get_string(decrypt_opts.at("/O").first);
    md5_update_exc(&ctx, get_user_pad(o_val).data(), 32);
    array<unsigned char, 4> ext = get_ext(decrypt_opts);
    md5_update_exc(&ctx, ext.data(), 4);
    string document_id = get_string(decrypt_opts.at("/ID").first);
    vector<unsigned char> doc_id(document_id.length());
    if (document_id.length() > 0)
    {
        doc_id = string2array(document_id);
        md5_update_exc(&ctx, doc_id.data(), document_id.length());
    }

    // If document metadata is not being encrypted, 
	// pass 4 bytes with the value 0xFFFFFFFF to the MD5 hash function.

    if (!is_encrypt_metadata(decrypt_opts))
    {
		unsigned char no_meta_addition[4] = { 0xff, 0xff, 0xff, 0xff };
        md5_update_exc(&ctx, no_meta_addition, 4);
	}

    unsigned char digest[MD5_DIGEST_LENGTH];
    int status;
    status = MD5_Final(digest, &ctx);
    if(status != 1) throw pdf_error(FUNC_STRING + "Error MD5-hashing data" );
    
    unsigned int revision = strict_stoul(decrypt_opts.at("/R").first);
    // only use the really needed bits as input for the hash
    if (revision == 3 || revision == 4)
    {
        for (int k = 0; k < 50; ++k)
        {
            md5_init_exc(&ctx);
            md5_update_exc(&ctx, digest, key_length);
            status = MD5_Final(digest, &ctx);
            if(status != 1) throw pdf_error(FUNC_STRING + "Error MD5-hashing data" );
        }
    }
    vector<unsigned char> encryption_key(key_length);
    memcpy(encryption_key.data(), digest, key_length);
    unsigned char user_key[32];

    // Setup user key
    if (revision == 3 || revision == 4)
    {
        md5_init_exc(&ctx);
        md5_update_exc(&ctx, padding, 32);
        if (!document_id.length() > 0) md5_update_exc(&ctx, doc_id.data(), document_id.length());
        status = MD5_Final(digest, &ctx);
        if(status != 1) throw pdf_error(FUNC_STRING + "Error MD5-hashing data");
        memcpy(user_key, digest, 16);
        for (int k = 16; k < 32; ++k) user_key[k] = 0;
        for (int k = 0; k < 20; k++)
        {
            for (int j = 0; j < key_length; ++j) digest[j] = static_cast<unsigned char>(encryption_key[j] ^ k);
            RC4(digest, key_length, user_key, 16, user_key);
        }
    }
    else
    {
        RC4(encryption_key.data(), key_length, padding, 32, user_key);
    }

    return encryption_key;
}

void get_MD5_binary(const unsigned char* data, int length, unsigned char* digest)
{
    MD5_CTX ctx;
    md5_init_exc(&ctx);
    md5_update_exc(&ctx, data, length);
    if (MD5_Final(digest, &ctx) != 1) throw pdf_error(FUNC_STRING + "error MD5_Final");
}

encrypt_algorithm_t get_algorithm(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    const string val = decrypt_opts.at("/V").first;
    switch (strict_stoul(val))
    {
    case 1:
        return ENCRYPT_ALGORITHM_RC4V1;
        break;
    case 2:
        return ENCRYPT_ALGORITHM_RC4V2;
        break;
    case 3:
        return ENCRYPT_ALGORITHM_AESV2;
        break;
    case 4:
        return ENCRYPT_ALGORITHM_AESV3;
        break;
    default:
        throw pdf_error(FUNC_STRING + "wrong /V value:" + val);
        break;
    }
}

void rc4_create_obj_key(unsigned int n,
                        unsigned int g,
                        unsigned char obj_key[MD5_DIGEST_LENGTH],
                        int *key_len,
                        const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    unsigned char nkey[MD5_DIGEST_LENGTH + 5 + 4];
    const vector<unsigned char> encryption_key = get_encryption_key(decrypt_opts);
    int local_key_len = encryption_key.size() + 5;

    for (size_t j = 0; j < encryption_key.size(); j++) nkey[j] = encryption_key[j];
    nkey[encryption_key.size() + 0] = static_cast<unsigned char>(0xff &  n);
    nkey[encryption_key.size() + 1] = static_cast<unsigned char>(0xff & (n >> 8));
    nkey[encryption_key.size() + 2] = static_cast<unsigned char>(0xff & (n >> 16));
    nkey[encryption_key.size() + 3] = static_cast<unsigned char>(0xff &  g);
    nkey[encryption_key.size() + 4] = static_cast<unsigned char>(0xff & (g >> 8));

	if (get_algorithm(decrypt_opts) == ENCRYPT_ALGORITHM_AESV2)
    {
        // AES encryption needs some 'salt'
        local_key_len += 4;
        nkey[encryption_key.size() + 5] = 0x73;
        nkey[encryption_key.size() + 6] = 0x41;
        nkey[encryption_key.size() + 7] = 0x6c;
        nkey[encryption_key.size() + 8] = 0x54;
    }
    
    get_MD5_binary(nkey, local_key_len, obj_key);
    *key_len = (encryption_key.size() <= 11) ? encryption_key.size() + 5 : 16;
}

string decrypt_rc4(unsigned int n,
                   unsigned int g,
                   const string &in_str,
                   const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    vector<unsigned char> in = string2array(in_str);
    unsigned char obj_key[MD5_DIGEST_LENGTH];
    int key_len;

    rc4_create_obj_key(n, g, obj_key, &key_len, decrypt_opts);
    vector<unsigned char> out_str;
    int out_len = key_len * 8 == 40? EVP_CIPHER_block_size(EVP_rc4_40()) + in.size():
                                     EVP_CIPHER_block_size(EVP_rc4()) + in.size();
    out_str.insert(out_str.end(), out_len, 0);
    out_len = RC4(obj_key, key_len, in.data(), in.size(), out_str.data());

    return string(reinterpret_cast<char*>(out_str.data()), out_len);
}
