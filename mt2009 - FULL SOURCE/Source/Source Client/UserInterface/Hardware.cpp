#include "stdafx.h"
#include <smbios.hpp>
#include <xorstr.h>
#include <ranges>
#include "Packet.h"
#include <cryptopp/cryptlib.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/aes.h>
#include <cryptopp/hex.h>

#include <boost/algorithm/string/trim.hpp>
#include <ThemidaSDK.h>

inline bool GetCPUID(std::string& sChecksum)
{
	unsigned long s1 = 0, s2 = 0, s3 = 0, s4 = 0;
	__asm
	{
		mov eax, 00h
		xor edx, edx
		cpuid
		mov s1, edx
		mov s2, eax
	}
	__asm
	{
		mov eax, 01h
		xor ecx, ecx
		xor edx, edx
		cpuid
		mov s3, edx
		mov s4, ecx
	}

	static char buf[64];
	sprintf_s(buf, "%08X%08X%08X%08X", s1, s2, s3, s4);
	sChecksum += buf;
#ifdef _DEBUG
	std::cout << "CPU ID: " << buf << std::endl;
#endif
	return true;
}

inline bool GetHDDID(std::string& sChecksum)
{
	DWORD dwUUID{};
	if (!GetVolumeInformationA("C://", NULL, NULL, &dwUUID, NULL, NULL, NULL, NULL))
		return false;

	sChecksum += std::to_string(dwUUID);
#ifdef _DEBUG
	std::cout << "HD ID: " << dwUUID << std::endl;
#endif
	return true;
}

inline bool GetMBID(std::string& sChecksum)
{
	using namespace smbios;
	DWORD smbios_data_size{};
	RawSMBIOSData* smbios_data{ nullptr };
	std::unique_ptr<BYTE[]> smbios_data_buffer{ new BYTE[0x10000] };

	smbios_data_size = GetSystemFirmwareTable('RSMB', 0, 0, 0);
	if (!smbios_data_size)
		return false;

	auto smbios_buffersize = smbios_data_size;
	smbios_data_size = GetSystemFirmwareTable('RSMB', 0, smbios_data_buffer.get(), smbios_buffersize);

	if (!smbios_data_size)
		return false;

	smbios_data = reinterpret_cast<RawSMBIOSData*>(smbios_data_buffer.get());
	if (smbios_data->Length != smbios_buffersize - 8)
		return false;

	auto b_ = smbios_data->SMBIOSTableData;
	auto pRange = std::ranges::iota_view{ (DWORD)0, smbios_data->Length };

	bool bDone{ false };
	std::for_each(pRange.begin(), pRange.end(), [&b_, &sChecksum, &smbios_data, &bDone](const auto&)
		{
			if (bDone)
				return;

			if (dmi_header* header = reinterpret_cast<dmi_header*> (b_); header->type == dmi_entry_type::DMI_ENTRY_BASEBOARD)
			{
				// Serial Number
				if (std::string sStr = dmi_string(header, b_[0x7]); sStr.find("Default string") == std::string::npos)
				{
					sChecksum += sStr;
#ifdef _DEBUG
					std::cout << "Serial Number: " << sStr << std::endl;
#endif
				}
				// UUID
				sChecksum += dmi_system_uuid(b_ + 0x8, smbios_data->SMBIOSMajorVersion * 0x100 + smbios_data->SMBIOSMinorVersion);
#ifdef _DEBUG
				std::cout << "UUID: " << dmi_system_uuid(b_ + 0x8, smbios_data->SMBIOSMajorVersion * 0x100 + smbios_data->SMBIOSMinorVersion) << std::endl;
#endif
				bDone = true;
			}
			else
			{
				b_ += header->length;
				while ((*(WORD*)b_) != 0) { b_++; }
				b_ += 2;
			}
		});

	return bDone;
}

std::string GetHardwareID()
{
	std::string sChecksum = "";

	bool exceptionFound = false;
	const bool getCPU = GetCPUID(sChecksum);
	const bool getHD = GetHDDID(sChecksum);
	const bool getMB = GetMBID(sChecksum);

	if (!getCPU)
	{
#ifdef _DEBUG
		std::cout << "Cannot get CPU ID!" << std::endl;
#endif
	}

	if (!getHD)
	{
#ifdef _DEBUG
		std::cout << "Cannot get HD ID!" << std::endl;
#endif
	}

	if (!getMB)
	{
#ifdef _DEBUG
		std::cout << "Cannot get MB ID!" << std::endl;
#endif
	}

	if (!getCPU && !getHD && !getMB)
	{
		sChecksum = "UNKNOWN";
		exceptionFound = true;
	}

#if defined(__THEMIDA__) && !defined(_DEBUG)
	VM_START
#endif

	// Strip from whitemarks and add size at the beginning of string
	boost::algorithm::trim(sChecksum);
	// Limit the output
	if (sChecksum.size() > HWID_MAX_LEN) sChecksum.resize((HWID_MAX_LEN));
#ifdef _DEBUG
	std::cout << "Full HWID ID: " << sChecksum << std::endl;
#endif



	if (sChecksum.size() < 1)
		sChecksum = "UNKNOWN";

	// Encrypting with AES
	// Setting encryption mode and output
	std::string encKey_{ XorStr("VBlk2yQRtiHA1oj7") };

	using namespace CryptoPP;
	std::string sCSEncr;
	ECB_Mode< AES >::Encryption CPEncryption;
	CPEncryption.SetKey((BYTE*)encKey_.data(), encKey_.size());

	try
	{
		// Encrypt string
		StringSource ss(sChecksum, true,
			new StreamTransformationFilter(CPEncryption,
				new StringSink(sCSEncr)
			) // StreamTransformationFilter      
		); // StringSource

		// Hex is to avoid string malversations
		sChecksum.clear();
		StringSource ss1(sCSEncr, true,
			new HexEncoder(
				new StringSink(sChecksum)
			) // HexEncoder
		); // StringSource
	}
	catch (const CryptoPP::Exception& e)
	{
		exceptionFound = true;
	}

#if defined(__THEMIDA__) && !defined(_DEBUG)
	VM_END
#endif

	if (exceptionFound)
		return "";

#ifdef _DEBUG
	std::cout << "Encrypted HWID ID(SIZE " << sChecksum.size() << ") : " << sChecksum << std::endl;
	{
		// Setting decryption mode and output
		std::string sCSDecr;
		ECB_Mode< AES >::Decryption CPDecryption;
		CPDecryption.SetKey((BYTE*)encKey_.data(), encKey_.size());

		// Decrypting
		try
		{
			// Dehex string
			sCSEncr.clear();
			StringSource ss(sChecksum, true,
				new HexDecoder(
					new StringSink(sCSEncr)
				) // HexEncoder
			); // StringSource

			// Decrypt string
			StringSource ss1(sCSEncr, true,
				new StreamTransformationFilter(CPDecryption,
					new StringSink(sCSDecr)
				) // StreamTransformationFilter      
			); // StringSource
		}
		catch (const CryptoPP::Exception& e)
		{
			std::cout << e.what() << std::endl;
			return "";
		}

		std::cout << "Decrypted HWID ID: " << sCSDecr << std::endl;
	}
#else
#ifdef __THEMIDA__
	//VM_END
#endif
#endif

	return sChecksum;
}


// Files shared by GameCore.top
