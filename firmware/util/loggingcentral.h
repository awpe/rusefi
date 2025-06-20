/**
 * @file	loggingcentral.h
 *
 * @date Mar 8, 2015
 * @author Andrey Belomutskiy, (c) 2012-2020
 */
#pragma once

#include <cstddef>
#include "generated_lookup_meta.h"
#include <utility>
#include <array>

class Logging;

void startLoggingProcessor();

const char* swapOutputBuffers(size_t *actualOutputBufferSize);

namespace priv
{
	// internal implementation, use efiPrintf below
	void efiPrintfInternal(const char *fmt, ...)
		#if EFI_PROD_CODE
			__attribute__ ((format (printf, 1, 2)))
		#endif
			;
}

#include <utility>

template <std::size_t N>
struct ConstStr {
	char data[N]{};

	consteval explicit ConstStr(const char (&str)[N]) {
		for (std::size_t i = 0; i < N; ++i)
			data[i] = str[i];
	}

	[[nodiscard]] static consteval std::size_t size() { return N - 1; }  // exclude null
};

template <std::size_t N1, std::size_t N2>
consteval auto concatFormat(ConstStr<N1> prefix, ConstStr<N2> fmt) {
	ConstStr delimiter{LOG_DELIMITER};
	// -2 and then +2 for code to be expressive
	constexpr std::size_t total = N1 + N2 + 2 * decltype(delimiter)::size() - 2; // don't count null-term for N1 and N2
	std::array<char, total + 1> result{}; // Needs one byte for null-term

	std::size_t pos = 0;
	for (std::size_t i = 0; i < N1 - 1; ++i) result[pos++] = prefix.data[i];
	for (std::size_t i = 0; i < decltype(delimiter)::size(); ++i) result[pos++] = delimiter.data[i];
	for (std::size_t i = 0; i < N2 - 1; ++i) result[pos++] = fmt.data[i];
	for (std::size_t i = 0; i < decltype(delimiter)::size(); ++i) result[pos++] = delimiter.data[i];
	result[pos] = '\0';

	return result;
}

template <ConstStr Fmt, ConstStr ProtoMsg, typename... Args>
void efiPrintfImpl(Args&&... args) {
	static constexpr auto fullFmt = concatFormat(
		ProtoMsg,
		Fmt
	);
	priv::efiPrintfInternal(fullFmt.data(), std::forward<Args>(args)...);
}

// "normal" logging messages need a header and footer, so put them in
// the format string at compile time
#define efiPrintfProto(proto, fmt, ...) efiPrintfImpl<ConstStr{proto}, ConstStr{fmt}>(__VA_ARGS__)
#define efiPrintf(fmt, ...) efiPrintfImpl<ConstStr{PROTOCOL_MSG}, ConstStr{fmt}>(__VA_ARGS__)

/**
 * This is the legacy function to copy the contents of a local Logging object in to the output buffer
 */
void scheduleLogging(Logging *logging);

// Stores the result of one call to efiPrintfInternal in the queue to be copied out to the output buffer
struct LogLineBuffer {
	char buffer[256];
};

template <size_t TBufferSize>
class LogBuffer {
public:
	void writeLine(LogLineBuffer* line);
	void writeLogger(Logging* logging);

	size_t length() const;
	void reset();
	const char* get() const;

#if !EFI_UNIT_TEST
private:
#endif
	void writeInternal(const char* buffer);

	char m_buffer[TBufferSize];
	char* m_writePtr = m_buffer;
};
