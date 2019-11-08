#include <cstring>
#include <map>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/md5.h>

#include "pdf_internal.h"



#include <iostream> //remove
using namespace std;

namespace
{
    const unsigned char padding[] =
        "\x28\xBF\x4E\x5E\x4E\x75\x8A\x41\x64\x00\x4E\x56\xFF\xFA\x01\x08\x2E\x2E\x00\xB6\xD0\x68\x3E\x80\x2F\x0C\xA9\xFE\x64\x53\x69\x7A";

    const unsigned char no_meta_addition[] = { 0xff, 0xff, 0xff, 0xff };

    typedef enum {
        ENCRYPT_ALGORITHM_RC4V1 = 1, ///< RC4 Version 1 encryption using a 40bit key
        ENCRYPT_ALGORITHM_RC4V2 = 2, ///< RC4 Version 2 encryption using a key with 40-128bit
        ENCRYPT_ALGORITHM_AESV2 = 4,  ///< AES encryption with a 128 bit key (PDF1.6)
        ENCRYPT_ALGORITHM_IDENTITY = 8 ///No encryption
    } encrypt_algorithm_t;

    typedef enum {
        PDF_KEY_LENGTH_128 = 128,
        PDF_KEY_LENGTH_256 = 256
    } pdf_key_length_t;

    enum { DEFAULT_LENGTH = 40, AES_IV_LENGTH = 16 };


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

    vector<unsigned char> string2array(const string &src)
    {
        vector<unsigned char> result(src.length());
        for (size_t j = 0; j < src.length(); j++) result[j] = static_cast<unsigned char>(src[j]);

        return result;
    }

    unsigned int get_length(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        auto it = decrypt_opts.find("/Length");
        if (it == decrypt_opts.end()) return DEFAULT_LENGTH / 8;

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
        if (MD5_Init(ctx) != 1) throw pdf_error(FUNC_STRING + "error initializing MD5 hashing engine" );
    }

    void md5_update_exc(MD5_CTX *ctx, const void *data, unsigned long len)
    {
        if (MD5_Update(ctx, data, len) != 1) throw pdf_error(FUNC_STRING + "error MD5 update" );
    }

    void md5_final_exc(unsigned char *md, MD5_CTX *ctx)
    {
        if (MD5_Final(md, ctx) != 1) throw pdf_error(FUNC_STRING + "error MD5 final");
    }

    void get_md5_binary(const unsigned char* data, int length, unsigned char* digest)
    {
        MD5_CTX ctx;
        md5_init_exc(&ctx);
        md5_update_exc(&ctx, data, length);
        md5_final_exc(digest, &ctx);
    }

    vector<unsigned char> get_decryption_key(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        unsigned int key_length = get_length(decrypt_opts);
        MD5_CTX ctx;
        md5_init_exc(&ctx);

        md5_update_exc(&ctx, padding, 32);
        const string o_val = decode_string(decrypt_opts.at("/O").first);
        md5_update_exc(&ctx, get_user_pad(o_val).data(), 32);

        array<unsigned char, 4> ext = get_ext(decrypt_opts);
        md5_update_exc(&ctx, ext.data(), 4);
        //get_first_array_element
        size_t offset = skip_spaces(decrypt_opts.at("/ID").first, 1);
        string document_id = decode_string(get_string(decrypt_opts.at("/ID").first, offset));
        vector<unsigned char> doc_id(document_id.length());
        if (document_id.length() > 0)
        {
            doc_id = string2array(document_id);
            md5_update_exc(&ctx, doc_id.data(), doc_id.size());
        }

        // If document metadata is not being encrypted,
        // pass 4 bytes with the value 0xFFFFFFFF to the MD5 hash function.
        if (!is_encrypt_metadata(decrypt_opts))
        {
            md5_update_exc(&ctx, no_meta_addition, sizeof(no_meta_addition)/sizeof(unsigned char));
        }

        unsigned char digest[MD5_DIGEST_LENGTH];
        md5_final_exc(digest, &ctx);

        unsigned int revision = strict_stoul(decrypt_opts.at("/R").first);
        // only use the really needed bits as input for the hash
        if (revision == 3 || revision == 4)
        {
            for (int k = 0; k < 50; ++k) get_md5_binary(digest, key_length, digest);
        }
        vector<unsigned char> decryption_key(key_length);
        memcpy(decryption_key.data(), digest, key_length);
        unsigned char user_key[32];

        // Setup user key
        if (revision == 3 || revision == 4)
        {
            md5_init_exc(&ctx);
            md5_update_exc(&ctx, padding, 32);
            if (!document_id.length() > 0) md5_update_exc(&ctx, doc_id.data(), document_id.length());
            md5_final_exc(digest, &ctx);
            memcpy(user_key, digest, 16);
            for (int k = 16; k < 32; ++k) user_key[k] = 0;
            for (int k = 0; k < 20; k++)
            {
                for (int j = 0; j < key_length; ++j) digest[j] = static_cast<unsigned char>(decryption_key[j] ^ k);
                RC4(digest, key_length, user_key, 16, user_key);
            }
        }
        else
        {
            RC4(decryption_key.data(), key_length, padding, 32, user_key);
        }

        return decryption_key;
    }

    encrypt_algorithm_t get_algorithm(const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        const string val = decrypt_opts.at("/R").first;
        switch (strict_stoul(val))
        {
        case 2:
        {
            return ENCRYPT_ALGORITHM_RC4V1;
            break;
        }
        case 3:
        {
            return ENCRYPT_ALGORITHM_RC4V2;
            break;
        }
        case 4:
        {
            if (decrypt_opts.count("/CF") == 0) return ENCRYPT_ALGORITHM_IDENTITY;
            const map<string, pair<string, pdf_object_t>> CF_dict = get_dictionary_data(decrypt_opts.at("/CF").first, 0);
            if (CF_dict.count("/StdCF") == 0) return ENCRYPT_ALGORITHM_IDENTITY;
            const map<string, pair<string, pdf_object_t>> stdCF_dict = get_dictionary_data(CF_dict.at("/StdCF").first, 0);
            auto it = stdCF_dict.find("/CFM");
            if (it == stdCF_dict.end()) return ENCRYPT_ALGORITHM_IDENTITY;
            //TODO: add /None support(custom decryption algorithm)
            if (it->second.first == "/V2") return ENCRYPT_ALGORITHM_RC4V2;
            if (it->second.first == "/AESV2") ENCRYPT_ALGORITHM_AESV2;
            throw pdf_error(FUNC_STRING + "wrong /CFM value:" + it->second.first);
            break;
        }
        default:
        {
            throw pdf_error(FUNC_STRING + "wrong /R value:" + val);
            break;
        }
        }
    }

    void create_obj_key(unsigned int n,
                        unsigned int g,
                        unsigned char obj_key[MD5_DIGEST_LENGTH],
                        int *key_len,
                        const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        unsigned char nkey[MD5_DIGEST_LENGTH + 5 + 4];
        const vector<unsigned char> decryption_key = get_decryption_key(decrypt_opts);

        int local_key_len = decryption_key.size() + 5;

        for (size_t j = 0; j < decryption_key.size(); j++) nkey[j] = decryption_key[j];
        nkey[decryption_key.size() + 0] = static_cast<unsigned char>(0xff &  n);
        nkey[decryption_key.size() + 1] = static_cast<unsigned char>(0xff & (n >> 8));
        nkey[decryption_key.size() + 2] = static_cast<unsigned char>(0xff & (n >> 16));
        nkey[decryption_key.size() + 3] = static_cast<unsigned char>(0xff &  g);
        nkey[decryption_key.size() + 4] = static_cast<unsigned char>(0xff & (g >> 8));

        if (get_algorithm(decrypt_opts) == ENCRYPT_ALGORITHM_AESV2)
        {
            // AES encryption needs some 'salt'
            local_key_len += 4;
            nkey[decryption_key.size() + 5] = 0x73;
            nkey[decryption_key.size() + 6] = 0x41;
            nkey[decryption_key.size() + 7] = 0x6c;
            nkey[decryption_key.size() + 8] = 0x54;
        }

        get_md5_binary(nkey, local_key_len, obj_key);
        *key_len = (decryption_key.size() <= 11) ? decryption_key.size() + 5 : 16;
    }

    string decrypt_rc4(unsigned int n,
                       unsigned int g,
                       const string &in_str,
                       const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        vector<unsigned char> in = string2array(in_str);
        unsigned char obj_key[MD5_DIGEST_LENGTH];
        int key_len;

        create_obj_key(n, g, obj_key, &key_len, decrypt_opts);
        vector<unsigned char> out_str;
        int out_len = key_len * 8 == 40? EVP_CIPHER_block_size(EVP_rc4_40()) + in.size():
        EVP_CIPHER_block_size(EVP_rc4()) + in.size();
        out_str.insert(out_str.end(), out_len, 0);
        out_len = RC4(obj_key, key_len, in.data(), in.size(), out_str.data());

        return string(reinterpret_cast<char*>(out_str.data()), out_len);
    }

    void aes_base_decrypt(const unsigned char* key,
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
        if ((text_len % 16) != 0) throw pdf_error(FUNC_STRING + "error: AES data length must be multiple of 16" );
        int status;
        switch (key_len)
        {
        case PDF_KEY_LENGTH_128 / 8:
            status = EVP_DecryptInit_ex(&aes, EVP_aes_128_cbc(), NULL, key, iv);
            break;
        case PDF_KEY_LENGTH_256 / 8:
            status = EVP_DecryptInit_ex(&aes, EVP_aes_256_cbc(), NULL, key, iv);
            break;
        default:
            throw pdf_error(FUNC_STRING + "invalid AES key length: " + to_string(key_len));
            break;
        }
        if(status != 1) throw pdf_error(FUNC_STRING + "error initializing AES decryption engine" );

        int data_out_moved;
        status = EVP_DecryptUpdate(&aes, text_out, &data_out_moved, text_in, text_len);
        out_len = data_out_moved;
        if(status != 1) throw pdf_error(FUNC_STRING + "Error AES-decryption data" );

        status = EVP_DecryptFinal_ex(&aes, text_out + out_len, &data_out_moved);
        out_len += data_out_moved;
        if(status != 1) throw pdf_error(FUNC_STRING + "Error AES-decryption data final");
    }

    string decrypt_aesv2(unsigned int n,
                         unsigned int g,
                         const string &in_str,
                         const map<string, pair<string, pdf_object_t>> &decrypt_opts)
    {
        vector<unsigned char> in = string2array(in_str);
        unsigned char obj_key[MD5_DIGEST_LENGTH];
        int key_len;

        create_obj_key(n, g, obj_key, &key_len, decrypt_opts);
        int out_buffer_len = in.size() - 2 - AES_IV_LENGTH;
        vector<unsigned char> out_buffer(out_buffer_len + 16 - (out_buffer_len % 16));
        aes_base_decrypt(obj_key,
                         key_len,
                         in.data(),
                         in.data() + AES_IV_LENGTH,
                         in.size() - AES_IV_LENGTH,
                         out_buffer.data(), out_buffer_len);
        return string(reinterpret_cast<char*>(out_buffer.data()), out_buffer_len);
    }
}

string decrypt(unsigned int n,
               unsigned int g,
               const string &in_str,
               const map<string, pair<string, pdf_object_t>> &decrypt_opts)
{
    if (decrypt_opts.empty()) return in_str;
    encrypt_algorithm_t algorithm = get_algorithm(decrypt_opts);
    switch (algorithm)
    {
    case ENCRYPT_ALGORITHM_RC4V1:
    case ENCRYPT_ALGORITHM_RC4V2:
        return decrypt_rc4(n, g, in_str, decrypt_opts);
        break;
    case ENCRYPT_ALGORITHM_AESV2:
        return decrypt_aesv2(n, g, in_str, decrypt_opts);
    case ENCRYPT_ALGORITHM_IDENTITY:
        return in_str;
    default:
        throw pdf_error("Unknown algorithm: " + to_string(algorithm));
        break;
    }
}
