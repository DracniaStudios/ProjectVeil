#include "Protocol.h"

#include <serialize.h>

#include <cmath>
#include <cstring>

// Protocol v1 on serialize 2.1.3 (MultiplayerSplitPlan.md §5.3, rules P-1 to P-4).
//
//   - Every enum is serialize_int(v, 0, Count-1) (P-4).
//   - serialize_uint64 is used only for Hello's worldHash (P-4).
//   - serialize_end closes every top-level message (P-4).
//   - Slots travel in [0, kMaxPlayers-1] and are checked against maxPlayers by
//     the receiver (P-8); station indices travel in [0, stationCount-1] (S-2).

namespace Net {

	namespace {

		constexpr int kTypeBits = 8;
		constexpr int kVersionBits = 16;

		// ---- Fields ---------------------------------------------------------

		template <typename Stream>
		bool SerializeHeader(Stream& stream, Header& header)
		{
			std::uint32_t type = static_cast<std::uint32_t>(header.type);
			std::uint32_t version = header.version;
			serialize_bits(stream, type, kTypeBits);
			serialize_bits(stream, version, kVersionBits);
			if (Stream::IsReading)
			{
				header.type = static_cast<MsgType>(type);
				header.version = static_cast<std::uint16_t>(version);
			}
			return true;
		}

		template <typename Stream, typename Enum>
		bool SerializeEnum(Stream& stream, Enum& value, int count)
		{
			int v = static_cast<int>(value);
			serialize_int(stream, v, 0, count - 1);
			if (Stream::IsReading) { value = static_cast<Enum>(v); }
			return true;
		}

		template <typename Stream>
		bool SerializeSlot(Stream& stream, int& slot)
		{
			serialize_int(stream, slot, 0, Sim::kMaxPlayers - 1);
			return true;
		}

		template <typename Stream>
		bool SerializeStationIndex(Stream& stream, int& index, const Limits& limits)
		{
			// A table with no stations has no valid index: nothing to send or accept
			if (limits.stationCount <= 0) { return serialize::serialize_fail(stream, serialize::REFUSAL_VALUE_OUT_OF_RANGE); }
			serialize_int(stream, index, 0, limits.stationCount - 1);
			return true;
		}

		template <typename Stream>
		bool SerializeCommand(Stream& stream, Sim::PlayerCommand& command)
		{
			std::uint32_t sequence = command.sequence;
			std::uint32_t clientTick = command.clientTick;
			int moveX = command.moveX;
			int moveY = command.moveY;
			std::uint32_t buttons = command.buttons & Sim::kAllButtons;
			serialize_bits(stream, sequence, 32);
			serialize_bits(stream, clientTick, 32);
			serialize_int(stream, moveX, -1, 1);
			serialize_int(stream, moveY, -1, 1);
			serialize_bits(stream, buttons, 10);
			serialize_float(stream, command.yawDelta);
			serialize_float(stream, command.pitchDelta);
			if (Stream::IsReading)
			{
				command.sequence = sequence;
				command.clientTick = clientTick;
				command.moveX = static_cast<std::int8_t>(moveX);
				command.moveY = static_cast<std::int8_t>(moveY);
				command.buttons = static_cast<std::uint16_t>(buttons);
			}
			return true;
		}

		// ---- Messages -------------------------------------------------------

		template <typename Stream>
		bool SerializeBody(Stream& stream, HelloMsg& m, const Limits&)
		{
			// Hello's prefix is frozen (P-1): worldHash, then the name
			serialize_uint64(stream, m.worldHash);
			int length = static_cast<int>(m.name.size());
			serialize_int(stream, length, 0, kMaxNameBytes);
			std::uint8_t name[kMaxNameBytes] = {};
			if (Stream::IsWriting) { std::memcpy(name, m.name.data(), static_cast<std::size_t>(length)); }
			if (length > 0) { serialize_bytes(stream, name, length); }
			if (Stream::IsReading) { m.name.assign(reinterpret_cast<const char*>(name), static_cast<std::size_t>(length)); }
			return true;
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, WelcomeMsg& m, const Limits&)
		{
			if (!SerializeSlot(stream, m.slot)) { return false; }
			serialize_int(stream, m.tickRate, 1, 1000);
			serialize_int(stream, m.maxPlayers, 1, Sim::kMaxPlayers);
			return true;
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, RejectMsg& m, const Limits&)
		{
			// Frozen (P-1): every version reads a v1 Reject the same way
			return SerializeEnum(stream, m.reason, static_cast<int>(RejectReason::Count));
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, CommandsMsg& m, const Limits&)
		{
			serialize_int(stream, m.count, 1, kMaxCommandsPerMessage);
			for (int i = 0; i < m.count; ++i)
			{
				if (!SerializeCommand(stream, m.commands[static_cast<std::size_t>(i)])) { return false; }
			}
			return true;
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, SnapshotMsg& m, const Limits& limits)
		{
			serialize_bits(stream, m.serverTick, 32);
			serialize_bits(stream, m.ackSeq, 32);
			serialize_bits(stream, m.roundEpoch, 32);

			serialize_int(stream, m.playerCount, 0, Sim::kMaxPlayers);
			for (int i = 0; i < m.playerCount; ++i)
			{
				SnapshotPlayer& p = m.players[static_cast<std::size_t>(i)];
				if (!SerializeSlot(stream, p.slot)) { return false; }
				serialize_float(stream, p.x);
				serialize_float(stream, p.y);
				serialize_float(stream, p.z);
				serialize_float(stream, p.yaw);
				serialize_float(stream, p.pitch);
				serialize_bool(stream, p.flashlight);
				serialize_bool(stream, p.crouching);
			}

			serialize_bool(stream, m.hasStalker);
			if (m.hasStalker)
			{
				serialize_float(stream, m.stalker.x);
				serialize_float(stream, m.stalker.y);
				serialize_float(stream, m.stalker.z);
				serialize_float(stream, m.stalker.yaw);
				serialize_int(stream, m.stalker.state, 0, kStalkerStateCount - 1);
				if (!SerializeEnum(stream, m.stalker.gait, static_cast<int>(StalkerGait::Count))) { return false; }
			}

			// Stations travel in StationTable order (S-1). The count is sent so a
			// receiver with another table refuses the whole Snapshot.
			serialize_int(stream, m.stationCount, 0, kMaxStations);
			if (Stream::IsReading && m.stationCount != limits.stationCount)
			{
				return serialize::serialize_fail(stream, serialize::REFUSAL_VALUE_OUT_OF_RANGE);
			}
			for (int i = 0; i < m.stationCount; ++i)
			{
				SnapshotStation& s = m.stations[static_cast<std::size_t>(i)];
				serialize_bool(stream, s.completed);
				serialize_int(stream, s.holder, -1, Sim::kMaxPlayers - 1);
			}
			return true;
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, StationRequestMsg& m, const Limits& limits)
		{
			if (!SerializeEnum(stream, m.action, static_cast<int>(StationAction::Count))) { return false; }
			return SerializeStationIndex(stream, m.stationIndex, limits);
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, StationEventMsg& m, const Limits& limits)
		{
			if (!SerializeEnum(stream, m.kind, static_cast<int>(Sim::StationEventKind::Count))) { return false; }
			if (!SerializeStationIndex(stream, m.stationIndex, limits)) { return false; }
			return SerializeSlot(stream, m.slot);
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, PlayerCaughtMsg& m, const Limits&)
		{
			return SerializeSlot(stream, m.slot);
		}

		template <typename Stream>
		bool SerializeBody(Stream& stream, RoundOverMsg& m, const Limits&)
		{
			// Ongoing is never sent: a RoundOver always ends a round
			int outcome = static_cast<int>(m.outcome);
			serialize_int(stream, outcome, static_cast<int>(Sim::RoundOutcome::Caught), static_cast<int>(Sim::RoundOutcome::Count) - 1);
			if (Stream::IsReading) { m.outcome = static_cast<Sim::RoundOutcome>(outcome); }
			serialize_bits(stream, m.roundEpoch, 32);
			return true;
		}

		// A whole message: header, body, end
		template <typename Stream>
		bool SerializeMessage(Stream& stream, Header& header, Message& message, const Limits& limits)
		{
			if (!SerializeHeader(stream, header)) { return false; }
			const bool ok = std::visit([&](auto& body) { return SerializeBody(stream, body, limits); }, message);
			if (!ok) { return false; }
			serialize_end(stream);
			return true;
		}

		// ---- Checks before encoding (P-3, P-4, P-9) ------------------------

		float Finite(float value, int& replaced)
		{
			if (std::isfinite(value)) { return value; }
			++replaced;
			return 0.0f;
		}

		bool ValidSlot(int slot) { return Sim::IsValidSlot(slot); }
		bool ValidIndex(int index, const Limits& limits) { return index >= 0 && index < limits.stationCount; }

		bool Check(HelloMsg& m, const Limits&, int&) { return IsValidName(m.name); }

		bool Check(WelcomeMsg& m, const Limits&, int&)
		{
			return ValidSlot(m.slot) && m.slot < m.maxPlayers && m.maxPlayers >= 1 && m.maxPlayers <= Sim::kMaxPlayers
				&& m.tickRate >= 1 && m.tickRate <= 1000;
		}

		bool Check(RejectMsg& m, const Limits&, int&) { return static_cast<int>(m.reason) < static_cast<int>(RejectReason::Count); }

		bool Check(CommandsMsg& m, const Limits&, int& replaced)
		{
			if (m.count < 1 || m.count > kMaxCommandsPerMessage) { return false; }
			for (int i = 0; i < m.count; ++i)
			{
				Sim::PlayerCommand& c = m.commands[static_cast<std::size_t>(i)];
				if (c.moveX < -1 || c.moveX > 1 || c.moveY < -1 || c.moveY > 1) { return false; }
				c.yawDelta = Finite(c.yawDelta, replaced);
				c.pitchDelta = Finite(c.pitchDelta, replaced);
			}
			return true;
		}

		bool Check(SnapshotMsg& m, const Limits& limits, int& replaced)
		{
			if (m.playerCount < 0 || m.playerCount > Sim::kMaxPlayers) { return false; }
			if (m.stationCount != limits.stationCount || m.stationCount < 0 || m.stationCount > kMaxStations) { return false; }
			for (int i = 0; i < m.playerCount; ++i)
			{
				SnapshotPlayer& p = m.players[static_cast<std::size_t>(i)];
				if (!ValidSlot(p.slot)) { return false; }
				p.x = Finite(p.x, replaced); p.y = Finite(p.y, replaced); p.z = Finite(p.z, replaced);
				p.yaw = Finite(p.yaw, replaced); p.pitch = Finite(p.pitch, replaced);
			}
			if (m.hasStalker)
			{
				if (m.stalker.state < 0 || m.stalker.state >= kStalkerStateCount) { return false; }
				if (static_cast<int>(m.stalker.gait) >= static_cast<int>(StalkerGait::Count)) { return false; }
				m.stalker.x = Finite(m.stalker.x, replaced); m.stalker.y = Finite(m.stalker.y, replaced);
				m.stalker.z = Finite(m.stalker.z, replaced); m.stalker.yaw = Finite(m.stalker.yaw, replaced);
			}
			for (int i = 0; i < m.stationCount; ++i)
			{
				const int holder = m.stations[static_cast<std::size_t>(i)].holder;
				if (holder < -1 || holder >= Sim::kMaxPlayers) { return false; }
			}
			return true;
		}

		bool Check(StationRequestMsg& m, const Limits& limits, int&)
		{
			return static_cast<int>(m.action) < static_cast<int>(StationAction::Count) && ValidIndex(m.stationIndex, limits);
		}

		bool Check(StationEventMsg& m, const Limits& limits, int&)
		{
			return static_cast<int>(m.kind) < static_cast<int>(Sim::StationEventKind::Count) && ValidIndex(m.stationIndex, limits) && ValidSlot(m.slot);
		}

		bool Check(PlayerCaughtMsg& m, const Limits&, int&) { return ValidSlot(m.slot); }

		bool Check(RoundOverMsg& m, const Limits&, int&)
		{
			return m.outcome == Sim::RoundOutcome::Caught || m.outcome == Sim::RoundOutcome::Escaped;
		}

		bool CheckAndClean(Message& message, const Limits& limits, int& replaced)
		{
			return std::visit([&](auto& m) { return Check(m, limits, replaced); }, message);
		}

		// After decoding: the rules the stream's ranges can't express
		bool CheckDecoded(const Message& message)
		{
			if (const auto* hello = std::get_if<HelloMsg>(&message)) { return IsValidName(hello->name); }
			if (const auto* welcome = std::get_if<WelcomeMsg>(&message)) { return welcome->slot < welcome->maxPlayers; }
			return true;
		}

		Message EmptyMessageOf(MsgType type)
		{
			switch (type)
			{
			case MsgType::Hello:          return HelloMsg{};
			case MsgType::Welcome:        return WelcomeMsg{};
			case MsgType::Reject:         return RejectMsg{};
			case MsgType::Commands:       return CommandsMsg{};
			case MsgType::Snapshot:       return SnapshotMsg{};
			case MsgType::StationRequest: return StationRequestMsg{};
			case MsgType::StationEvent:   return StationEventMsg{};
			case MsgType::PlayerCaught:   return PlayerCaughtMsg{};
			case MsgType::RoundOver:      return RoundOverMsg{};
			}
			return HelloMsg{};
		}
	}

	const char* RejectReasonName(RejectReason reason)
	{
		switch (reason)
		{
		case RejectReason::VersionMismatch: return "VersionMismatch";
		case RejectReason::WorldMismatch:   return "WorldMismatch";
		case RejectReason::ServerFull:      return "ServerFull";
		default:                            return "Unknown";
		}
	}

	const char* DecodeErrorName(DecodeError error)
	{
		switch (error)
		{
		case DecodeError::None:            return "None";
		case DecodeError::TooShort:        return "TooShort";
		case DecodeError::TooLong:         return "TooLong";
		case DecodeError::UnknownType:     return "UnknownType";
		case DecodeError::VersionMismatch: return "VersionMismatch";
		case DecodeError::Malformed:       return "Malformed";
		case DecodeError::OutOfRange:      return "OutOfRange";
		}
		return "Unknown";
	}

	MsgType TypeOf(const Message& message)
	{
		return static_cast<MsgType>(kFirstMsgType + static_cast<int>(message.index()));
	}

	bool IsValidName(const std::string& name)
	{
		if (name.size() > static_cast<std::size_t>(kMaxNameBytes)) { return false; }
		for (const char c : name)
		{
			if (c < 0x20 || c > 0x7e) { return false; }
		}
		return true;
	}

	bool PeekHeader(std::span<const std::byte> bytes, Header& out)
	{
		if (bytes.size() < kHeaderBytes) { return false; }
		const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());

		// The header is the first 24 bits, written low bit first: type in the low 8,
		// version in the next 16
		const std::uint32_t word = static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) | (static_cast<std::uint32_t>(data[2]) << 16);
		const int type = static_cast<int>(word & 0xFF);
		if (type < kFirstMsgType || type > kLastMsgType) { return false; }
		out.type = static_cast<MsgType>(type);
		out.version = static_cast<std::uint16_t>((word >> 8) & 0xFFFF);
		return true;
	}

	std::span<const std::byte> Encoder::Encode(const Message& message, const Limits& limits)
	{
		floatsReplaced = 0;
		Message copy = message;
		if (!CheckAndClean(copy, limits, floatsReplaced)) { return {}; }

		serialize::WriteStream stream(buffer.data(), static_cast<int64_t>(buffer.size()));
		Header header{ TypeOf(copy), kProtocolVersion };
		if (!SerializeMessage(stream, header, copy, limits)) { return {}; }
		stream.Flush();

		const auto bytes = static_cast<std::size_t>(stream.GetBytesProcessed());
		if (bytes < kHeaderBytes || bytes > kMaxMessage) { return {}; }
		return std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer.data()), bytes);
	}

	DecodeError Decoder::Decode(std::span<const std::byte> bytes, const Limits& limits, Message& out, Header* headerOut)
	{
		if (bytes.size() < kHeaderBytes) { return DecodeError::TooShort; }
		if (bytes.size() > kMaxMessage) { return DecodeError::TooLong; }

		Header header;
		if (!PeekHeader(bytes, header)) { return DecodeError::UnknownType; }
		if (headerOut) { *headerOut = header; }

		// Only Reject is read across versions: its layout is frozen (P-1)
		if (header.version != kProtocolVersion && header.type != MsgType::Reject) { return DecodeError::VersionMismatch; }

		// Never a ReadStream over the transport's buffer: the reader loads up to 7
		// bytes past the data, so the payload is copied here with 8 bytes of slack
		serialize::ReadStream stream;
		if (!stream.InitializePadded(scratch.data(), static_cast<int64_t>(scratch.size()),
			reinterpret_cast<const std::uint8_t*>(bytes.data()), static_cast<int64_t>(bytes.size())))
		{
			return DecodeError::TooLong;
		}

		Message decoded = EmptyMessageOf(header.type);
		Header readHeader;
		if (!SerializeMessage(stream, readHeader, decoded, limits)) { return DecodeError::Malformed; }
		if (!CheckDecoded(decoded)) { return DecodeError::OutOfRange; }

		out = std::move(decoded);
		return DecodeError::None;
	}

	std::size_t MeasureMaxBytes(MsgType type)
	{
		Limits limits;
		limits.stationCount = kMaxStations;
		Message message = EmptyMessageOf(type);

		if (auto* hello = std::get_if<HelloMsg>(&message)) { hello->name.assign(kMaxNameBytes, 'x'); }
		if (auto* commands = std::get_if<CommandsMsg>(&message)) { commands->count = kMaxCommandsPerMessage; }
		if (auto* snapshot = std::get_if<SnapshotMsg>(&message))
		{
			snapshot->playerCount = Sim::kMaxPlayers;
			for (int i = 0; i < Sim::kMaxPlayers; ++i) { snapshot->players[static_cast<std::size_t>(i)].slot = i; }
			snapshot->hasStalker = true;
			snapshot->stationCount = kMaxStations;
		}

		serialize::MeasureStream stream;
		Header header{ type, kProtocolVersion };
		SerializeMessage(stream, header, message, limits);
		return static_cast<std::size_t>(stream.GetBytesProcessed());
	}
}
