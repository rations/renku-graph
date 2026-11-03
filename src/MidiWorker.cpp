/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "MidiWorker.h"

#include <List.h>
#include <MessageRunner.h>
#include <MidiConsumer.h>
#include <MidiProducer.h>
#include <MidiRoster.h>

#include <stdio.h>
#include <string.h>

#include "Messages.h"
#include "Snapshot.h"


static const bigtime_t kScanDelay = 100000;


MidiWorker::MidiWorker(const BMessenger& target)
	:
	BLooper("midi worker"),
	fTarget(target),
	fScanRunner(NULL)
{
}


MidiWorker::~MidiWorker()
{
	delete fScanRunner;
}


void
MidiWorker::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgWorkerStart:
		{
			// StartWatching() replays every existing endpoint and connection
			// as B_MIDI_EVENT messages, which schedules the first scan.
			BMessenger self(this);
			BMidiRoster::StartWatching(&self);
			_ScheduleScan();
			break;
		}

		case B_MIDI_EVENT:
			_ScheduleScan();
			break;

		case kMsgScanTimer:
			delete fScanRunner;
			fScanRunner = NULL;
			_Scan();
			break;

		case kMsgScanNow:
			_Scan();
			break;

		case kMsgConnectMidi:
			_Connect(message, true);
			break;

		case kMsgDisconnectMidi:
			_Connect(message, false);
			break;

		default:
			BLooper::MessageReceived(message);
	}
}


void
MidiWorker::_ScheduleScan()
{
	if (fScanRunner != NULL)
		return;

	BMessage timer(kMsgScanTimer);
	fScanRunner = new(std::nothrow) BMessageRunner(BMessenger(this), &timer,
		kScanDelay, 1);
	if (fScanRunner != NULL && fScanRunner->InitCheck() != B_OK) {
		delete fScanRunner;
		fScanRunner = NULL;
	}
}


void
MidiWorker::_Scan()
{
	MidiSnapshot* snapshot = new(std::nothrow) MidiSnapshot;
	if (snapshot == NULL)
		return;

	// NextEndpoint() returns registered remote endpoints with a reference
	// the caller must release (src/kits/midi2/MidiRoster.cpp).
	int32 id = 0;
	BMidiEndpoint* endpoint;
	while ((endpoint = BMidiRoster::NextEndpoint(&id)) != NULL) {
		MidiEndpointEntry entry;
		entry.id = endpoint->ID();
		entry.name = endpoint->Name();
		entry.producer = endpoint->IsProducer();

		BMessage properties;
		if (endpoint->GetProperties(&properties) == B_OK) {
			const char* device;
			if (properties.FindString("device", &device) == B_OK)
				entry.device = device;
		}

		if (entry.producer) {
			// Connections() returns a new list of acquired consumers
			// (src/kits/midi2/MidiProducer.cpp).
			BMidiProducer* producer = static_cast<BMidiProducer*>(endpoint);
			BList* connections = producer->Connections();
			if (connections != NULL) {
				for (int32 i = 0; i < connections->CountItems(); i++) {
					BMidiConsumer* consumer
						= static_cast<BMidiConsumer*>(
							connections->ItemAt(i));
					entry.connections.push_back(consumer->ID());
					consumer->Release();
				}
				delete connections;
			}
		}

		snapshot->endpoints.push_back(entry);
		endpoint->Release();
	}

	BMessage message(kMsgMidiSnapshot);
	message.AddPointer("snapshot", snapshot);
	if (fTarget.SendMessage(&message) != B_OK)
		delete snapshot;
}


void
MidiWorker::_Connect(BMessage* message, bool connect)
{
	BMidiProducer* producer
		= BMidiRoster::FindProducer(message->GetInt32("producer", -1));
	BMidiConsumer* consumer
		= BMidiRoster::FindConsumer(message->GetInt32("consumer", -1));

	if (producer == NULL || consumer == NULL) {
		_Status("Could not change the MIDI connection: the endpoint is "
			"gone.");
	} else {
		status_t error = connect
			? producer->Connect(consumer) : producer->Disconnect(consumer);
		char text[512];
		if (error != B_OK) {
			snprintf(text, sizeof(text), "Could not %s \"%s\" and \"%s\": %s",
				connect ? "connect" : "disconnect", producer->Name(),
				consumer->Name(), strerror(error));
		} else {
			snprintf(text, sizeof(text), "%s \"%s\" %s \"%s\".",
				connect ? "Connected" : "Disconnected", producer->Name(),
				connect ? "to" : "from", consumer->Name());
		}
		_Status(text);
	}

	if (producer != NULL)
		producer->Release();
	if (consumer != NULL)
		consumer->Release();

	_ScheduleScan();
}


void
MidiWorker::_Status(const char* text)
{
	BMessage message(kMsgStatus);
	message.AddString("text", text);
	fTarget.SendMessage(&message, (BHandler*)NULL, 1000000);
}
