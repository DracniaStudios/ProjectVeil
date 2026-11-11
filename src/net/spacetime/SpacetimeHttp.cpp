// cpp-httplib brings in <winsock2.h> and <windows.h>. Nothing from raylib may be
// included in this file: raylib's CloseWindow, DrawText and Rectangle collide
// with windows.h (rule R8 keeps this translation unit to HTTP and JSON only).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <httplib.h>

#include "SpacetimeHttp.h"

#include <fstream>
#include <system_error>

namespace Net::Spacetime {

	struct HttpClient::Impl
	{
		Impl(std::string host, int port) : client(std::move(host), port)
		{
			// Short timeouts: the worker retries, so a slow answer is better
			// abandoned than waited on. No set_logger, ever: it would log the token.
			client.set_connection_timeout(2, 0);
			client.set_read_timeout(5, 0);
			client.set_write_timeout(5, 0);
			client.set_keep_alive(true);
		}

		httplib::Client client;
	};

	namespace {
		HttpResult FromResult(const httplib::Result& result)
		{
			HttpResult out;
			if (!result)
			{
				out.transportOk = false;
				out.transportError = httplib::to_string(result.error());
				return out;
			}
			out.transportOk = true;
			out.status = result->status;
			out.body = result->body;
			return out;
		}

		httplib::Headers Bearer(std::string_view token)
		{
			httplib::Headers headers;
			if (!token.empty()) { headers.emplace("Authorization", "Bearer " + std::string(token)); }
			return headers;
		}
	}

	HttpClient::HttpClient(std::string host, int port) : impl(std::make_unique<Impl>(std::move(host), port)) {}
	HttpClient::~HttpClient() = default;

	HttpResult HttpClient::MintIdentity()
	{
		return FromResult(impl->client.Post("/v1/identity"));
	}

	HttpResult HttpClient::CallReducer(std::string_view database, std::string_view reducer, const nlohmann::json& arguments, std::string_view token)
	{
		const std::string path = "/v1/database/" + std::string(database) + "/call/" + std::string(reducer) + "?confirmed=true";
		return FromResult(impl->client.Post(path, Bearer(token), arguments.dump(), "application/json"));
	}

	HttpResult HttpClient::Sql(std::string_view database, std::string_view query, std::string_view token)
	{
		const std::string path = "/v1/database/" + std::string(database) + "/sql";
		return FromResult(impl->client.Post(path, Bearer(token), std::string(query), "text/plain"));
	}

	void HttpClient::Stop()
	{
		impl->client.stop();
	}

	bool ParseIdentity(std::string_view body, Identity& out)
	{
		const nlohmann::json json = nlohmann::json::parse(body, nullptr, false);
		if (json.is_discarded() || !json.is_object()) { return false; }
		if (!json.contains("identity") || !json["identity"].is_string()) { return false; }
		if (!json.contains("token") || !json["token"].is_string()) { return false; }
		out.identity = json["identity"].get<std::string>();
		out.token = json["token"].get<std::string>();
		return !out.identity.empty() && !out.token.empty();
	}

	IdentityFile LoadIdentity(const std::filesystem::path& file, Identity& out)
	{
		std::error_code error;
		if (!std::filesystem::exists(file, error)) { return error ? IdentityFile::Unreadable : IdentityFile::Missing; }

		std::ifstream in(file, std::ios::binary);
		if (!in) { return IdentityFile::Unreadable; }
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return ParseIdentity(text, out) ? IdentityFile::Loaded : IdentityFile::Unreadable;
	}

	bool SaveIdentity(const std::filesystem::path& file, const Identity& identity)
	{
		std::error_code error;
		std::filesystem::create_directories(file.parent_path(), error);

		std::filesystem::path temp = file;
		temp += ".tmp";
		{
			std::ofstream out(temp, std::ios::binary | std::ios::trunc);
			if (!out) { return false; }
			out << nlohmann::json{ { "identity", identity.identity }, { "token", identity.token } }.dump(1, '\t');
			out.flush();
			if (!out) { return false; }
		}

		std::filesystem::rename(temp, file, error);
		if (error)
		{
			std::filesystem::remove(temp, error);
			return false;
		}
		return true;
	}

	Obtained ObtainIdentity(HttpClient& client, const std::filesystem::path& file, Identity& out, std::string& error)
	{
		switch (LoadIdentity(file, out))
		{
		case IdentityFile::Loaded:
			return Obtained::Reused;

		case IdentityFile::Unreadable:
			error = "the identity file " + file.string() + " exists but can't be read; fix or delete it by hand";
			return Obtained::Failed;

		case IdentityFile::Missing:
			break;
		}

		const HttpResult minted = client.MintIdentity();
		if (!minted.transportOk) { error = "minting an identity failed: " + minted.transportError; return Obtained::Failed; }
		if (minted.status != 200) { error = "minting an identity answered HTTP " + std::to_string(minted.status); return Obtained::Failed; }
		if (!ParseIdentity(minted.body, out)) { error = "minting an identity returned an unexpected body"; return Obtained::Failed; }
		if (!SaveIdentity(file, out)) { error = "the new identity couldn't be written to " + file.string(); return Obtained::Failed; }
		return Obtained::Minted;
	}

	namespace {
		bool ParseOneResult(const nlohmann::json& result, SqlResult& out)
		{
			if (!result.is_object() || !result.contains("schema") || !result.contains("rows")) { return false; }

			const nlohmann::json& schema = result["schema"];
			if (!schema.is_object() || !schema.contains("elements") || !schema["elements"].is_array()) { return false; }
			for (const auto& element : schema["elements"])
			{
				// {"name":{"some":"world_key"}, ...}
				if (!element.is_object() || !element.contains("name")) { return false; }
				const auto& name = element["name"];
				if (name.is_object() && name.contains("some") && name["some"].is_string()) { out.columns.push_back(name["some"].get<std::string>()); }
				else { out.columns.emplace_back(); }
			}

			if (!result["rows"].is_array()) { return false; }
			for (const auto& row : result["rows"])
			{
				if (!row.is_array() || row.size() != out.columns.size()) { return false; }
				out.rows.push_back(row);
			}
			return true;
		}
	}

	bool ParseSqlResults(std::string_view body, std::vector<SqlResult>& out)
	{
		out.clear();
		const nlohmann::json json = nlohmann::json::parse(body, nullptr, false);
		if (json.is_discarded() || !json.is_array() || json.empty()) { return false; }
		for (const auto& result : json)
		{
			SqlResult parsed;
			if (!ParseOneResult(result, parsed)) { out.clear(); return false; }
			out.push_back(std::move(parsed));
		}
		return true;
	}

	bool ParseSqlRows(std::string_view body, std::vector<std::string>& columns, std::vector<nlohmann::json>& rows)
	{
		columns.clear();
		rows.clear();
		std::vector<SqlResult> results;
		if (!ParseSqlResults(body, results)) { return false; }
		columns = std::move(results[0].columns);
		rows = std::move(results[0].rows);
		return true;
	}
}
