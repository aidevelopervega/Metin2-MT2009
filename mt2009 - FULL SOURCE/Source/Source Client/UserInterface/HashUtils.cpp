#include "stdafx.h"
#include "HashUtils.h"

#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/modes.h>

// Function to encrypt a string using AES
std::string encryptAESString(const std::string& plainText, const std::string& key) {
    std::string cipherText;

    // Create an AES encryptor in ECB mode
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption encryptor(
        reinterpret_cast<const byte*>(key.data()),
        key.size()
    );

    // Perform the encryption
    CryptoPP::StringSource(plainText, true,
        new CryptoPP::StreamTransformationFilter(encryptor,
            new CryptoPP::StringSink(cipherText)
        )
    );

    return cipherText;
}

// Function to decrypt a string using AES
std::string decryptAESString(const std::string& cipherText, const std::string& key) {
    std::string decryptedText;

    // Create an AES decryptor in ECB mode
    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption decryptor(
        reinterpret_cast<const byte*>(key.data()),
        key.size()
    );

    // Perform the decryption
    CryptoPP::StringSource(cipherText, true,
        new CryptoPP::StreamTransformationFilter(decryptor,
            new CryptoPP::StringSink(decryptedText)
        )
    );

    return decryptedText;
}
// Files shared by GameCore.top
