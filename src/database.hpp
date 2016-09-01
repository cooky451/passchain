#pragma once

#include "utility/property_node.hpp"
#include "edit_distance.hpp"
#include "memory_reader.hpp"

#include "chacha/chacha.hpp"
#include "keccak/keccak.hpp"

#include <cctype>
#include <cmath>
#include <ctime>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <random>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>
#include <sstream>

/* File Format
 * All integers are stored in little endian.
 * 
 * encryption = chacha20 (with nonce = 0, nonce is already baked into key)
 * 
 * derive_key(password, nonce, domain) = {
 * 		auto h = sha3-256(password || nonce || domain);
 * 
 *		for (size_t i = 0; i < 10001; ++i) 
 *			h = sha3-256(password || h);
 * 
 *		return h;
 * }
 * 
 * 32 byte enc_key = derive_key(password, nonce, "ENC-KEY");
 * 32 byte mac_key = derive_key(password, nonce, "MAC-KEY");
 * 
 * 16 byte hash = truncate(sha3-256(everything after this)) (only for error detection)
 * 02 byte ffv (file format version, uint16_t)
 * 14 byte reserved (zeroed)
 * 32 byte nonce = randomly generated on store
 * 32 byte mac = sha3-256(key || ffv + reserved-bytes after ffv || encrypted_data)
 * ---- Encrypted ====
 * 08 byte int64_t timestamp (seconds since 1970)
 * 04 byte uint32_t number of database entries
 * 20 byte reserved (zeroed)
 * ---- N database entries ==== (Unaligned beyond this point.)
 * 8 byte uint64_t entry ID (randomly generated)
 * 8 byte int64_t timestamp (seconds since 1970)
 * 2 byte uint16_t number of snapshots
 * ---- N snapshots ====
 * 8 byte int64_t timestamp (seconds since 1970)
 * 2 byte uint16_t username length
 * N byte string username
 * 2 byte uint16_t password length
 * N byte string password
 * ==== End of snapshots ----
 * 2 byte uint16_t entry name length
 * N byte string entry name
 * 2 byte uint16_t comment length
 * N byte string comment
 * 2 byte uint16_t generator extra alphabet length
 * N byte string generator extra alphabet
 * 4 byte uint32_t generator password length
 * 2 byte uint16_t flags
 *  ^ generate letters flag ( & 1)
 *  ^ generate numbers flag ( & 2)
 *  ^ generate special characters flag ( & 4)
 *  ^ generate using extra alphabet flag ( & 8)
 *  ^ reserved flags...
 * ==== End of database ----
 * ==== End of encryption ----
 */

// Also assuming that std::time()'s epoch is 1970 and it's resolution are seconds.
// Sadly one cannot test for that.
static_assert(std::is_same<std::time_t, std::int64_t>::value, "time_t has wrong type");

static constexpr char asciiLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr char asciiNumbers[] = "0123456789";
static constexpr char asciiSpecial[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

typedef chacha::buffered_cipher Cipher;
typedef keccak::random_engine_256 RandomGenerator;
typedef keccak::sha3_256_hasher Hasher;

#if !defined(_MSC_VER) && !defined(__clang__)
static_assert(false, "Make sure your compiler honors volatile here.");
#endif

inline void volatileZeroMemory(volatile void* ptr, std::size_t size)
{
	for (auto bytePtr = static_cast<volatile std::uint8_t*>(ptr); size-- > 0; ++bytePtr)
	{
		*bytePtr = 0;
	}
}

inline void volatileTouchMemory(const volatile void* ptr, std::size_t size)
{
	std::uint8_t dummy;

	for (auto bytePtr = static_cast<const volatile std::uint8_t*>(ptr); size-- > 0; ++bytePtr)
	{
		dummy = *bytePtr;
	}
}

inline std::array<std::uint8_t, 32> deriveKey(const std::string& password,
	const std::array<std::uint8_t, 32>& nonce, const std::string& domain)
{
	Hasher hasher;
	hasher.update(password.data(), password.size());
	hasher.update(nonce.data(), nonce.size());
	hasher.update(domain.data(), domain.size());

	auto h = hasher.finish();

	for (size_t i = 0; i < 10001; ++i)
	{
		hasher.update(password.data(), password.size());
		hasher.update(h.data(), h.size());
		h = hasher.finish();
	}

	return h;
}

inline void transformString(const std::array<std::uint8_t, 32>& key, std::string& str, 
	std::uint64_t nonce, std::uint64_t startBlockIndex)
{
	chacha::unbuffered_cipher cipher(chacha::key_bits<256>(), key.data(), nonce);
	cipher.set_block_index(startBlockIndex);
	cipher.transform(&str[0], &str[0], str.size()); // str[0] is guaranteed to be valid!
}

class VolatileZeroGuard
{
	void* _ptr;
	std::size_t _size;

public:
	~VolatileZeroGuard()
	{
		volatileZeroMemory(_ptr, _size);
	}

	VolatileZeroGuard(void* ptr, std::size_t size)
		: _ptr(ptr)
		, _size(size)
	{}
};

class Snapshot
{
public:
	std::time_t timestamp;
	std::string username; // encrypted with temp key!
	std::string password; // encrypted with temp key!

	Snapshot() {}
	Snapshot(const Snapshot&) = delete;
	Snapshot(Snapshot&&) = default;
	Snapshot& operator = (const Snapshot&) = delete;
	Snapshot& operator = (Snapshot&&) = default;
};

class PasswordGeneratorDesc
{
public:
	std::string extraAlphabet;
	std::uint16_t passwordLength = 16;
	bool genLetters = true;
	bool genNumbers = true;
	bool genSpecial = false;
	bool genExtra = false;
};

class LoginData
{
public:
	std::uint64_t uniqueId;
	std::time_t timestamp;
	std::string name;
	std::string comment; // encrypted with temp key!
	std::vector<Snapshot> snapshots;
	PasswordGeneratorDesc generatorDesc;
	bool hide = false;
};

inline double calculateBitStrength(const PasswordGeneratorDesc& desc)
{
	double base = 0.0;

	if (desc.genLetters) base += sizeof asciiLetters - 1;
	if (desc.genNumbers) base += sizeof asciiNumbers - 1;
	if (desc.genSpecial) base += sizeof asciiSpecial - 1;
	if (desc.genExtra) base += desc.extraAlphabet.size();

	return desc.passwordLength * std::log2(base);
}

inline std::string generateAlphabet(const PasswordGeneratorDesc& desc)
{
	std::string alphabet;

	if (desc.genLetters) alphabet += asciiLetters;
	if (desc.genNumbers) alphabet += asciiNumbers;
	if (desc.genSpecial) alphabet += asciiSpecial;
	if (desc.genExtra) alphabet += desc.extraAlphabet;

	std::sort(alphabet.begin(), alphabet.end());
	alphabet.erase(std::unique(alphabet.begin(), alphabet.end()), alphabet.end());

	return alphabet;
}

inline std::string generatePassword(const PasswordGeneratorDesc& desc, RandomGenerator& rng)
{
	std::string alphabet = generateAlphabet(desc);

	if (alphabet.size() == 0)
	{
		throw std::invalid_argument("Alphabet doesn't contain any characters.");
	}

	for (auto c : alphabet)
	{
		if (!std::isprint(c) || std::isspace(c))
		{
			throw std::invalid_argument("Character in alphabet is not printable or whitespace."
				" Multibyte characters are not supported.");
		}
	}

	std::string randomPassword(desc.passwordLength, char());
	std::uniform_int_distribution<std::size_t> dist(0, alphabet.size() - 1);

	for (auto& c : randomPassword)
	{
		c = alphabet[dist(rng)];
	}

	return randomPassword;
}

inline void mergeLogins(LoginData& target, LoginData& source)
{
	if (target.timestamp > source.timestamp)
	{
		volatileZeroMemory(&target.comment[0], target.comment.size());

		target.timestamp = source.timestamp;
		target.name = std::move(source.name);
		target.comment = std::move(source.comment);
		target.hide = source.hide;
		target.generatorDesc = std::move(source.generatorDesc);
	}
	else
	{
		volatileZeroMemory(&source.comment[0], source.comment.size());
	}

	for (auto& sn : source.snapshots)
	{
		target.snapshots.push_back(std::move(sn));
	}

	std::sort(target.snapshots.begin(), target.snapshots.end(), 
		[](const Snapshot& lhs, const Snapshot& rhs) {
		return lhs.timestamp < rhs.timestamp;
	});

	auto end = std::unique(target.snapshots.begin(), target.snapshots.end(), 
		[](const Snapshot& lhs, const Snapshot& rhs) {
		return lhs.timestamp == rhs.timestamp
			&& lhs.username == rhs.username
			&& lhs.password == rhs.password;
	});

	for (auto it = end; it != target.snapshots.end(); ++it)
	{
		volatileZeroMemory(&it->username[0], it->username.size());
		volatileZeroMemory(&it->password[0], it->password.size());
	}

	target.snapshots.erase(end, target.snapshots.end());
	source.snapshots.clear();
}

class LoginDatabase
{
	class TransformGuard
	{
		LoginDatabase& _database;
		LoginData& _data;

	public:
		~TransformGuard()
		{
			_database.transformEntry(_data);
		}

		TransformGuard(LoginDatabase& database, LoginData& data)
			: _database(database)
			, _data(data)
		{
			_database.transformEntry(_data);
		}
	};

	static constexpr std::uint16_t FF_VER_MAJOR = 2;
	static constexpr std::uint16_t FF_VER_MINOR = 7;
	static constexpr std::uint16_t FF_VER = FF_VER_MAJOR << 8 | FF_VER_MINOR;

	std::array<std::uint8_t, 32> _tempKey;
	RandomGenerator _randomGenerator;
	std::string _password; // encrypted with temp key!
	std::vector<LoginData> _database;
	std::time_t _lastSerialize;
	std::thread _swapPreventionThread;
	std::atomic_bool _stopThread = false;

public:
	~LoginDatabase()
	{
		_stopThread = true;
		_swapPreventionThread.join();
		volatileZeroMemory(&_tempKey, sizeof _tempKey);
		volatileZeroMemory(&_randomGenerator, sizeof _randomGenerator);
	}

	LoginDatabase(LoginDatabase&&) = delete; // Prevents auto generation of move/copy operators.

	LoginDatabase(std::string password)
		: _password(std::move(password))
		, _randomGenerator(nullptr, 0)
	{
		auto s0 = std::time(nullptr);
		auto s1 = std::chrono::steady_clock::now();
		auto s2 = std::random_device()();
		auto s3 = std::random_device()();

		_randomGenerator.reseed(&s0, sizeof s0);
		_randomGenerator.reseed(&s1, sizeof s1);
		_randomGenerator.reseed(&s2, sizeof s2);
		_randomGenerator.reseed(&s3, sizeof s3);

		std::array<std::uint8_t, 32> tempKeyNonce;
		_randomGenerator.extract(&tempKeyNonce[0], 32);

		_tempKey = deriveKey(_password, tempKeyNonce, "TMP-KEY");

		transformString(_tempKey, _password, 0, 0);

		_swapPreventionThread = std::thread([this] {
			while (!this->_stopThread)
			{
				volatileTouchMemory(&this->_tempKey, sizeof this->_tempKey);
				volatileTouchMemory(&this->_randomGenerator, sizeof this->_randomGenerator);
				std::this_thread::sleep_for(std::chrono::milliseconds(80));
			}
		});
	}

	std::size_t countSnapshots() const
	{
		std::size_t counter = 0;

		for (auto& entry : _database)
		{
			for (auto& sn : entry.snapshots)
			{
				++counter;
			}
		}

		return counter;
	}

	std::uint64_t makeUniqueId()
	{
		std::uint64_t uniqueId;
		_randomGenerator.extract(&uniqueId, sizeof uniqueId);
		return uniqueId;
	}

	LoginData* getEntry(std::size_t index)
	{
		if (index < _database.size())
		{
			return &_database[index];
		}

		return nullptr;
	}

	LoginData* findEntry(std::uint64_t uniqueId)
	{
		for (auto& entry : _database)
		{
			if (entry.uniqueId == uniqueId)
			{
				return &entry;
			}
		}

		return nullptr;
	}

	LoginData* pushEntry(LoginData&& data)
	{
		_database.push_back(std::move(data));
		return &_database.back();
	}

	TransformGuard transformGuard(LoginData& data)
	{
		return TransformGuard(*this, data);
	}

	void transformEntry(LoginData& data)
	{
		transformString(_tempKey, data.comment, data.uniqueId, 0);

		for (std::size_t i = 0; i < data.snapshots.size(); ++i)
		{
			transformString(_tempKey, data.snapshots[i].username, data.uniqueId, (i + 1) * 0xFFFF);
			transformString(_tempKey, data.snapshots[i].password, data.uniqueId, (i + 1) * 0xFFFFFF);
		}
	}

	void reseedRng(const void* data, std::size_t size)
	{
		_randomGenerator.reseed(data, size);
	}

	std::string generatePassword(const PasswordGeneratorDesc& desc)
	{
		return ::generatePassword(desc, _randomGenerator);
	}

	void sort(const std::string& searchString)
	{
		std::sort(_database.begin(), _database.end(),
			[&](const LoginData& lhs, const LoginData& rhs) {

			// Hidden entries always have lower priority.
			if (lhs.hide != rhs.hide)
				return rhs.hide;

			// Calculate distances to search string
			auto ldistance = wordBasedEditDistance(searchString, lhs.name);
			auto rdistance = wordBasedEditDistance(searchString, rhs.name);

			if (ldistance != rdistance)
				return ldistance < rdistance;

			// If strings have the same distance, sort lexicographically
			return lhs.name < rhs.name;
		});
	}

	void mergeFromEncryptedFile(const std::string& filename)
	{
		auto aaa = readFileBinary(filename);
		std::vector<std::uint8_t> buffer(aaa.size());
		std::memcpy(buffer.data(), aaa.data(), buffer.size());

		VolatileZeroGuard bufferZeroGuard(buffer.data(), buffer.size());

		if (buffer.size() < 128)
		{
			throw std::runtime_error("Database file too small.");
		}

		std::array<std::uint8_t, 16> givenHash;
		std::memcpy(&givenHash[0], &buffer[0], 16);
		std::array<std::uint8_t, 16> actualHash;
		Hasher(&buffer[16], buffer.size() - 16).finish(&actualHash[0], 16);

		if (givenHash != actualHash)
		{
			throw std::runtime_error("File was damaged.");
		}

		std::uint16_t fileFormatVersion;
		std::memcpy(&fileFormatVersion, &buffer[16], sizeof fileFormatVersion);

		if (fileFormatVersion >> 8 != FF_VER_MAJOR)
		{
			throw std::runtime_error("Incompatible file format version.");
		}

		std::array<std::uint8_t, 32> nonce;
		std::memcpy(&nonce[0], &buffer[32], 32);
		transformString(_tempKey, _password, 0, 0);
		auto enckey = deriveKey(_password, nonce, "ENC-KEY");
		auto mackey = deriveKey(_password, nonce, "MAC-KEY");
		transformString(_tempKey, _password, 0, 0);
		VolatileZeroGuard keyZeroGuard(&enckey, sizeof enckey);
		VolatileZeroGuard macZeroGuard(&mackey, sizeof mackey);

		Hasher hasher(mackey.data(), mackey.size());
		hasher.update(&buffer[16], 16);
		hasher.update(&buffer[96], buffer.size() - 96);

		auto calculatedMac = hasher.finish();

		// No need to worry about timing attacks, correct MAC is obviously known to anyone.
		if (std::memcmp(&calculatedMac[0], &buffer[64], 32) != 0)
		{
			throw std::runtime_error("Wrong password.");
		}

		Cipher cipher(chacha::key_bits<256>(), enckey.data(), 0);
		cipher.transform(&buffer[96], &buffer[96], buffer.size() - 96);
		volatileZeroMemory(&cipher, sizeof cipher);

		std::uint32_t nEntries;

		std::memcpy(&_lastSerialize, &buffer[96], sizeof _lastSerialize);
		std::memcpy(&nEntries, &buffer[104], sizeof nEntries);

		MemoryReader memoryReader(buffer.data() + 128, buffer.size() - 128);

		const auto extractData = [&](void* buffer, std::size_t size)
		{
			if (!memoryReader.read(buffer, size))
			{
				throw std::runtime_error("Unexpected end of file while parsing database.");
			}
		};

		const auto extractString = [&]()
		{
			std::uint16_t size;
			extractData(&size, sizeof size);
			std::string str(size, char());
			extractData(&str[0], size); // &s[0] is always valid.
			return str;
		};

		while (nEntries--)
		{
			LoginData loginData;

			extractData(&loginData.uniqueId, sizeof loginData.uniqueId);
			extractData(&loginData.timestamp, sizeof loginData.timestamp);

			std::uint16_t nSnapshots;
			extractData(&nSnapshots, sizeof nSnapshots);

			for (std::size_t i = 0; i < nSnapshots; ++i)
			{
				Snapshot snapshot;
				extractData(&snapshot.timestamp, sizeof snapshot.timestamp);
				snapshot.username = extractString();
				snapshot.password = extractString();

				loginData.snapshots.push_back(std::move(snapshot));
			}

			loginData.name = extractString();
			loginData.comment = extractString();
			loginData.generatorDesc.extraAlphabet = extractString();
			extractData(&loginData.generatorDesc.passwordLength, sizeof loginData.generatorDesc.passwordLength);

			std::uint16_t flags;
			extractData(&flags, sizeof flags);

			loginData.generatorDesc.genLetters = (flags & 0x1) != 0;
			loginData.generatorDesc.genNumbers = (flags & 0x2) != 0;
			loginData.generatorDesc.genSpecial = (flags & 0x4) != 0;
			loginData.generatorDesc.genExtra = (flags & 0x8) != 0;
			loginData.hide = (flags & 0x10) != 0;

			transformEntry(loginData);
			_database.push_back(std::move(loginData));
		}
	}

	std::vector<std::uint8_t> serializeBinary()
	{
		_lastSerialize = std::time(nullptr);

		auto nEntries = static_cast<std::uint32_t>(std::min(std::size_t{ 0xFFFFFFFF }, _database.size()));

		std::vector<std::uint8_t> buffer(128);

		std::memcpy(&buffer[16], &FF_VER, sizeof FF_VER);
		_randomGenerator.extract(&buffer[32], 32); // Generating nonce
		std::memcpy(&buffer[96], &_lastSerialize, sizeof _lastSerialize);
		std::memcpy(&buffer[104], &nEntries, sizeof nEntries);

		const auto writeToBuffer = [&](const void* data, std::size_t size)
		{
			const auto bufferSize = buffer.size();
			buffer.resize(bufferSize + size);
			std::memcpy(buffer.data() + bufferSize, data, size);
		};

		const auto writeString = [&](const std::string& s)
		{
			const auto size = static_cast<std::uint16_t>(std::min(std::size_t{ 0xFFFF }, s.size()));
			writeToBuffer(&size, sizeof size);
			writeToBuffer(s.data(), size);
		};

		for (auto& entry : _database)
		{
			if (entry.snapshots.size() > 0xFFFF)
			{ // This will *never* happen in normal usage. (Nobody changes the password for a service over 65,000 times.)
				throw std::invalid_argument("Too many snapshots in database entry."
					" Please use text export and manually erase them.");
			}

			auto guard = transformGuard(entry);

			const auto nSnapshots = static_cast<std::uint16_t>(entry.snapshots.size());
			writeToBuffer(&entry.uniqueId, sizeof entry.uniqueId);
			writeToBuffer(&entry.timestamp, sizeof entry.timestamp);
			writeToBuffer(&nSnapshots, sizeof nSnapshots);

			for (std::size_t i = 0; i < nSnapshots; ++i)
			{
				auto& snapshot = entry.snapshots[i];
				writeToBuffer(&snapshot.timestamp, sizeof snapshot.timestamp);
				writeString(snapshot.username);
				writeString(snapshot.password);
			}

			writeString(entry.name);
			writeString(entry.comment);
			writeString(entry.generatorDesc.extraAlphabet);

			std::uint16_t flags = 0;
			flags |= (entry.generatorDesc.genLetters << 0u);
			flags |= (entry.generatorDesc.genNumbers << 1u);
			flags |= (entry.generatorDesc.genSpecial << 2u);
			flags |= (entry.generatorDesc.genExtra << 3u);
			flags |= (entry.hide << 4u);

			writeToBuffer(&entry.generatorDesc.passwordLength, sizeof entry.generatorDesc.passwordLength);
			writeToBuffer(&flags, sizeof flags);
		}

		// Derive keys
		std::array<std::uint8_t, 32> nonce;
		std::memcpy(&nonce[0], &buffer[32], 32);
		transformString(_tempKey, _password, 0, 0);
		auto enckey = deriveKey(_password, nonce, "ENC-KEY");
		auto mackey = deriveKey(_password, nonce, "MAC-KEY");
		transformString(_tempKey, _password, 0, 0);

		// Encrypt data
		Cipher cipher(chacha::key_bits<256>(), enckey.data(), 0);
		cipher.transform(&buffer[96], &buffer[96], buffer.size() - 96);

		// Calculate mac
		Hasher hasher(mackey.data(), mackey.size());
		hasher.update(&buffer[16], 16);
		hasher.update(&buffer[96], buffer.size() - 96);
		hasher.finish(&buffer[64], 32);

		volatileZeroMemory(&cipher, sizeof cipher);
		volatileZeroMemory(&enckey, sizeof enckey);
		volatileZeroMemory(&mackey, sizeof mackey);

		// Calculate hash to differentiate between a wrong password and a damaged file.
		Hasher(&buffer[16], buffer.size() - 16).finish(&buffer[0], 16);

		return buffer;
	}

	void mergeFromText(const char* text)
	{
		const auto extractLoginData = [&](PropertyNode& node)
		{
			LoginData data = {};
			node.loadValue("unique_id", data.uniqueId);
			node.loadValue("timestamp", data.timestamp);
			node.loadValue("name", data.name);
			node.loadValue("comment", data.comment);
			node.loadValue("hide", data.hide);
			node.loadValue("gen.letters", data.generatorDesc.genLetters);
			node.loadValue("gen.numbers", data.generatorDesc.genNumbers);
			node.loadValue("gen.special", data.generatorDesc.genSpecial);
			node.loadValue("gen.extra", data.generatorDesc.genExtra);
			node.loadValue("gen.length", data.generatorDesc.passwordLength);
			node.loadValue("gen.extra_alphabet", data.generatorDesc.extraAlphabet);

			for (auto& n : node.nodes())
			{
				Snapshot sn = {};
				n->loadValue("username", sn.username);
				n->loadValue("password", sn.password);
				n->loadValue("timestamp", sn.timestamp);

				data.snapshots.push_back(std::move(sn));
			}

			if (data.uniqueId == 0)
			{
				data.uniqueId = makeUniqueId();
			}

			if (data.timestamp == 0)
			{
				data.timestamp = std::time(nullptr);
			}

			return data;
		};

		PropertyNode node(nullptr, "");
		node.parse(text);

		for (auto& n : node.nodes())
		{
			auto data = extractLoginData(*n);
			auto existing = findEntry(data.uniqueId);

			if (existing != nullptr)
			{
				auto guard = transformGuard(*existing);
				mergeLogins(*existing, data);
			}
			else
			{
				transformEntry(data);
				_database.push_back(std::move(data));
			}
		}
	}

	std::string serializeText()
	{
		_lastSerialize = std::time(nullptr);

		PropertyNode node(nullptr, "");

		node.storeValue("number_of_entries", _database.size());
		node.storeValue("number_of_snapshots", countSnapshots());
		node.storeValue("last_serialize", _lastSerialize);

		for (auto& data : _database)
		{
			auto guard = transformGuard(data);

			auto n = node.appendNode(std::to_string(data.uniqueId));
			n->storeValue("unique_id", data.uniqueId);
			n->storeValue("timestamp", data.timestamp);
			n->storeValue("name", data.name);
			n->storeValue("comment", data.comment);
			n->storeValue("hide", data.hide);
			n->storeValue("gen.letters", data.generatorDesc.genLetters);
			n->storeValue("gen.numbers", data.generatorDesc.genNumbers);
			n->storeValue("gen.special", data.generatorDesc.genSpecial);
			n->storeValue("gen.extra", data.generatorDesc.genExtra);
			n->storeValue("gen.length", data.generatorDesc.passwordLength);
			n->storeValue("gen.extra_alphabet", data.generatorDesc.extraAlphabet);

			for (std::size_t i = 0; i < data.snapshots.size(); ++i)
			{
				auto nn = n->appendNode(std::to_string(i));
				nn->storeValue("username", data.snapshots[i].username);
				nn->storeValue("password", data.snapshots[i].password);
				nn->storeValue("timestamp", std::to_string(data.snapshots[i].timestamp));
			}
		}

		std::stringstream ss;
		node.save(ss);
		return ss.str();
	}
};
