#include <string>
#include <stdexcept>

#include "Execution.hpp"
#include "Roblox.hpp"
#include "Scheduler.hpp"

#include "zstd.h"
#include "xxhash.h"

#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeUtils.h"

const uint32_t identity = 8u, script = NULL;

class bytecode_encoder_t : public Luau::BytecodeEncoder {
	inline void encode(uint32_t* data, size_t count) override {
		// Loop through the instructions.
		for (auto i = 0u; i < count;) {
			// Get the opcode (which is the first byte of the instruction).
			auto& opcode = *reinterpret_cast<uint8_t*>(data + i);

			// Add the instruction length (which could be multiple 32-bit integers).
			i += Luau::getOpLength(LuauOpcode(opcode));

			// Encode the opcode.
			opcode *= 227;
		}
	}
};

std::string compress_bytecode(std::string_view bytecode) {
	// Create a buffer.
	const auto data_size = bytecode.size();
	const auto max_size = ZSTD_compressBound(data_size);
	auto buffer = std::vector<char>(max_size + 8);

	// Copy RSB1 and data size into the buffer.
	strcpy_s(&buffer[0], buffer.capacity(), "RSB1");
	memcpy_s(&buffer[4], buffer.capacity(), &data_size, sizeof(data_size));

	// Copy compressed bytecode into the buffer.
	const auto compressed_size = ZSTD_compress(&buffer[8], max_size, bytecode.data(), data_size, ZSTD_maxCLevel());
	if (ZSTD_isError(compressed_size))
		throw std::runtime_error("Failed to compress the bytecode.");

	// Encrypt the buffer.
	const auto size = compressed_size + 8;
	const auto key = XXH32(buffer.data(), size, 42u);
	const auto bytes = reinterpret_cast<const uint8_t*>(&key);

	for (auto i = 0u; i < size; ++i)
		buffer[i] ^= bytes[i % 4] + i * 41u;

	// Create and return output.
	return std::string(buffer.data(), size);
}

void Execution::execute_bytecode(std::string_view bytecode) {
	// Compress and load the bytecode.
	auto compressed = compress_bytecode(bytecode);
	const auto state = Roblox::get_state(Scheduler::get_script_context(), &identity, &script);

	if (Roblox::luavm_load(state, &compressed, "", 0))
		throw std::runtime_error("Unexpected error during execution.");
	
	// Set the identity, spawn the script closure and pop it off the stack.
	Roblox::set_identity(state, identity);
	Roblox::task_spawn(state);
	Roblox::pop_stack(state, 1);
}

void Execution::execute_script(const std::string& source) {
	// If the script is empty, return.
	if (source.length() == 0)
		return;

	// Compile the script.
	static auto encoder = bytecode_encoder_t();
	const auto bytecode = Luau::compile("task.wait()" + source, {}, {}, &encoder);

	// Print out the compiler error or execute the bytecode.
	if (bytecode[0] == '\0')
		Roblox::print(2, bytecode.c_str() + 1);
	else
		Execution::execute_bytecode(bytecode);
}
