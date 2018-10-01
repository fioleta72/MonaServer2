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


#include "Mona/STUN/STUNProtocol.h"
#include "Mona/BinaryWriter.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {


struct Decoder : Socket::Decoder, virtual Object {
	Decoder(const shared<Socket>& pSocket) : _pSocket(pSocket) {}

private:
	void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
		//DEBUG("STUN Message received from ", address, " size : ", pBuffer->size());

		if (pBuffer->size() < 4) {
			ERROR("Unexpected STUN message size ", pBuffer->size(), " from ", address);
			return;
		}
		BinaryReader reader(pBuffer->data(), pBuffer->size());
		UInt16 type = reader.read16();
		reader.next(2); // size
		switch (type) {
		case 0x01: { // Binding Request
			if (reader.available() != 16) {
				ERROR("Unexpected size ", reader.size(), " received in Binding Request from ", address);
				return;
			}
			string transactionId;
			reader.read(16, transactionId); // cookie & transaction ID

			shared<Buffer> pBufferOut(new Buffer());
			BinaryWriter writer(*pBufferOut);
			writer.write16(0x0101); // Binding Success response
			writer.write16(address.host().size() + 8); // size of the message
			writer.write(transactionId);

			writer.write16(0x0020); // XOR Mapped Address
			writer.write16(address.host().size() + 4); // size
			writer.write8(0); // reserved
			writer.write8(address.family() == IPAddress::IPv4 ? 1 : 2); // @ family
			writer.write16(address.port() ^ ((transactionId[0] << 8) + transactionId[1])); // port XOR 1st bytes of cookie

			const void* pAdd = address.host().data();
			for (int i = 0; i < address.host().size(); ++i)
				writer.write8((*((UInt8*)pAdd + i)) ^ transactionId[i]);

			Exception ex;
			AUTO_WARN(_pSocket->write(ex, Packet(pBufferOut), address), "STUN response");
			break;
		}
		default:
			WARN("Unknown message type ", type," from ", address);
			break;
		}
	}
	shared<Socket> _pSocket;
};

STUNProtocol::STUNProtocol(const char* name, ServerAPI& api, Sessions& sessions) : UDProtocol(name, api, sessions) {

	setNumber("port", 3478);
}

Socket::Decoder* STUNProtocol::newDecoder() { 
	return new Decoder(socket());
}

} // namespace Mona