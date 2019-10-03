/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/
#include "Mona/SRT/SRTProtocol.h"
#include "Mona/SRT/SRTSession.h"

using namespace std;

#if defined(SRT_API)

namespace Mona {

DataReader& SRTProtocol::Params::operator()() {
	DataReader::reset();
	return self;
}

bool SRTProtocol::Params::readOne(UInt8 type, DataWriter& writer) {
	// TODO: Handle nested keys? => #!:{...}
	if (reader.available() < 4 || String::ICompare(STR reader.current(), EXPAND("#!::")) != 0) {
		// direct resource!
		if (_path.set(string(STR reader.current(), reader.available())))
			return true;
		DEBUG("Invalid SRT streamid ", String::Data(reader.current(), 4));
		return false;
	}
	reader.next(4);

	writer.beginObject();
	// Read pair key=value
	const char* key = NULL;
	const char* value = NULL;
	do {
		const char* cur = STR reader.current();
		if (!reader.available() || *cur == ',') {
			if (!key)
				break; // nothing to read!
			if (!value)
				value = cur;
			const char* end = value;
			while (end > key && isblank(*(end - 1)))
				--end;
			String::Scoped scoped(end);
			// trim left the value
			while (isblank(*++value));
			// trim right the value
			end = cur;
			while (end > value && isblank(*(end - 1)))
				--end;
			write(writer, key, value, end - value);
			key = value = NULL;
		} else if (*cur == '=') {
			if (!key)
				key = ""; // empty key!
			if(!value)
				value = cur;
		} else if (!key && !isblank(*cur))
			key = cur;
	} while (reader.next());
	writer.endObject();
	return _done = true;
}

void SRTProtocol::Params::write(DataWriter& writer, const char* key, const char* value, UInt32 size) {
	writer.writeProperty(key, value, size);
	if (_done)
		return; // else already done!
	if (String::ICompare(key, "m") == 0) {
		if(String::ICompare(value, size, "request") == 0)
			_subscribe = true;
		else if (String::ICompare(value, size, "publish") == 0)
			_publish = true;
		// Note: Bidirectional is not supported for now as a socket cannot be subcribed twice
		// else if (String::ICompare(value, size, "bidirectional") == 0)
		//  publish = subscribe = true;
	} else if (String::ICompare(key, "r") == 0)
		_path.set(string(value, size));
}


SRTProtocol::SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions) : _server(api.ioSocket), Protocol(name, api, sessions) {
	setNumber("port", 9710);
	setNumber("timeout", 60); // 60 seconds

	_server.onConnection = [this](const shared<Socket>& pSocket) {
		// Try to read the parameter "streamid"
		Params params(((SRT::Socket&)*pSocket).stream());
		if (params) {
			if(params.stream().empty()) // raise just in the case where streamid=#!:: (without any resource)
				ERROR("SRT connection with streamid has to specify a valid resource (r parameter)")
			else
				this->sessions.create<SRTSession>(self, pSocket).init(params);
		} else
			ERROR("SRT connection without a valid streamid, use ini configuration rather to configure statically SRT input and output");

	};
	_server.onError = [this](const Exception& ex) {
		WARN("Protocol ", this->name, ", ", ex); // onError by default!
	};
}

bool SRTProtocol::load(Exception& ex) {
	if (!Protocol::load(ex))
		return false;
	return _server.start(ex, address);
}

} // namespace Mona

#endif
