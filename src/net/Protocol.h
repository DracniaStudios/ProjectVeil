#pragma once
#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

// Protocol v1 (MultiplayerSplitPlan.md §5.3, decisions Q42, Q55; rules P-1 to
// P-9). Nine messages, encoded with the vendored serialize 2.1.3 inside
// Protocol.cpp only: this header holds plain structs, so no wire-serialization
// type leaves src/net/ (rule R6). JSON stays for config files and saves.
//
// Bump kProtocolVersion whenever a message layout changes; a client and server
// with different versions refuse each other at the handshake (rule R5). The
// header, Hello's prefix and Reject's layout never change (P-1).

#include <sim/GameEvents.h>
#include <sim/PlayerCommand.h>
#include <sim/Rules.h>
#include <sim/Slots.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>

namespace Net {

	inline constexpr std::uint16_t kProtocolVersion = 1;
	inline constexpr std::uint16_t kDefaultPort     = 7777;
	inline constexpr int           kSnapshotRateHz  = 30;

	inline constexpr std::size_t kMaxMessage   = 2048; // every message, either way (P-2, P-7)
	inline constexpr std::size_t kHeaderBytes  = 3;    // {type u8, protocolVersion u16}
	inline constexpr int kMaxStations          = 64;   // refused above this when the World save loads (P-3)
	inline constexpr int kMaxNameBytes         = 24;   // printable ASCII (P-9)
	inline constexpr int kMaxCommandsPerMessage = 3;   // the last three, so one lost packet costs nothing
	inline constexpr int kStalkerStateCount    = 4;    // the locked FSM (Q32)

	enum class MsgType : std::uint8_t {
		Hello = 1,       // C→S reliable: worldHash, name
		Welcome,         // S→C reliable: slot, tickRate, maxPlayers
		Reject,          // S→C reliable, then close with linger: reason
		Commands,        // C→S unreliable: 1–3 PlayerCommands
		Snapshot,        // S→C unreliable, 30 Hz
		StationRequest,  // C→S reliable: Claim / Release / Complete, station index (no slot)
		StationEvent,    // S→C reliable: Granted / Denied / Released / Completed, index, slot
		PlayerCaught,    // S→C reliable: slot
		RoundOver,       // S→C reliable: outcome, roundEpoch
	};
	inline constexpr int kFirstMsgType = 1;
	inline constexpr int kLastMsgType  = 9;

	enum class RejectReason : std::uint8_t { VersionMismatch, WorldMismatch, ServerFull, Count };
	enum class StationAction : std::uint8_t { Claim, Release, Complete, Count };
	enum class StalkerGait : std::uint8_t { Still, Walk, Sprint, Count };

	const char* RejectReasonName(RejectReason reason);

	struct HelloMsg        { std::uint64_t worldHash = 0; std::string name; };
	struct WelcomeMsg      { int slot = 0; int tickRate = 60; int maxPlayers = Sim::kDefaultMaxPlayers; };
	struct RejectMsg       { RejectReason reason = RejectReason::VersionMismatch; };
	struct CommandsMsg     { int count = 0; std::array<Sim::PlayerCommand, kMaxCommandsPerMessage> commands = {}; };

	struct SnapshotPlayer
	{
		int   slot = 0;
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float yaw = 0.0f, pitch = 0.0f;
		bool  flashlight = false;
		bool  crouching = false;
	};

	struct SnapshotStalker
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float yaw = 0.0f;
		int   state = 0; // 0..kStalkerStateCount-1
		StalkerGait gait = StalkerGait::Still;
	};

	struct SnapshotStation
	{
		bool completed = false;
		int  holder = -1; // the Slot holding the claim, or -1
	};

	struct SnapshotMsg
	{
		std::uint32_t serverTick = 0; // never resets (P-8)
		std::uint32_t ackSeq = 0;     // the last command sequence applied for the receiver
		std::uint32_t roundEpoch = 0;
		int playerCount = 0;
		std::array<SnapshotPlayer, Sim::kMaxPlayers> players = {};
		bool hasStalker = false;
		SnapshotStalker stalker = {};
		int stationCount = 0; // must equal the StationTable's size on both ends
		std::array<SnapshotStation, kMaxStations> stations = {};
	};

	struct StationRequestMsg { StationAction action = StationAction::Claim; int stationIndex = 0; };
	struct StationEventMsg   { Sim::StationEventKind kind = Sim::StationEventKind::Granted; int stationIndex = 0; int slot = 0; };
	struct PlayerCaughtMsg   { int slot = 0; };
	struct RoundOverMsg      { Sim::RoundOutcome outcome = Sim::RoundOutcome::Caught; std::uint32_t roundEpoch = 0; };

	using Message = std::variant<HelloMsg, WelcomeMsg, RejectMsg, CommandsMsg, SnapshotMsg,
		StationRequestMsg, StationEventMsg, PlayerCaughtMsg, RoundOverMsg>;

	MsgType TypeOf(const Message& message);

	// What indices are checked against, on both ends (S-2): the StationTable's
	// size, which a matching World save makes the same everywhere
	struct Limits
	{
		int stationCount = 0;
	};

	enum class DecodeError : std::uint8_t {
		None,
		TooShort,         // under the 3-byte header (P-2)
		TooLong,          // over kMaxMessage (P-2)
		UnknownType,
		VersionMismatch,  // header carries another protocol version; header() still says which message
		Malformed,        // the stream refused, or bytes were left over (serialize_end)
		OutOfRange,       // decoded, but a count, slot, index or name is outside the rules
	};

	const char* DecodeErrorName(DecodeError error);

	struct Header
	{
		MsgType type = MsgType::Hello;
		std::uint16_t version = 0;
	};

	// Reads only the frozen header (P-1). False for fewer than 3 bytes or an
	// unknown type.
	bool PeekHeader(std::span<const std::byte> bytes, Header& out);

	/**
	 * Encodes one message (P-3): a fixed buffer, Serialize, Flush, a span over
	 * the bytes. Every count, slot, index, enum and name is checked first, since
	 * the writer has no bounds check in a Release build (review B5); a message
	 * that breaks a limit is refused with an empty span. Non-finite floats are
	 * replaced with 0 first (P-4); FloatsReplaced() counts them.
	 * The span stays valid until the next Encode.
	 */
	class Encoder
	{
	public:
		std::span<const std::byte> Encode(const Message& message, const Limits& limits);
		int FloatsReplaced() const { return floatsReplaced; }

	private:
		alignas(8) std::array<std::uint8_t, kMaxMessage> buffer = {};
		int floatsReplaced = 0;
	};

	/**
	 * Decodes one message (P-2): refuses under 3 or over 2048 bytes, copies the
	 * payload into a member buffer with 8 bytes of slack (the reader loads up to
	 * 7 bytes past the data), never reads a transport buffer directly, and
	 * decodes into owned structs. A message from another protocol version is
	 * refused as VersionMismatch, except Reject, whose layout is frozen.
	 */
	class Decoder
	{
	public:
		DecodeError Decode(std::span<const std::byte> bytes, const Limits& limits, Message& out, Header* header = nullptr);

	private:
		alignas(8) std::array<std::uint8_t, kMaxMessage + 8> scratch = {};
	};

	// The largest each message can be at the protocol's maximum counts, measured
	// with serialize's MeasureStream. For the N3 test and the bandwidth budget.
	std::size_t MeasureMaxBytes(MsgType type);

	// A name is at most 24 bytes of printable ASCII (P-9)
	bool IsValidName(const std::string& name);
}

#endif
