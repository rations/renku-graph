/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef MIDI_WORKER_H
#define MIDI_WORKER_H


#include <Looper.h>
#include <Messenger.h>


class BMessageRunner;


/*!	Owns the Midi Kit 2 calls: watches the midi_server roster, posts
	snapshots of every endpoint and connection, and connects/disconnects.
*/
class MidiWorker : public BLooper {
public:
								MidiWorker(const BMessenger& target);
	virtual						~MidiWorker();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_ScheduleScan();
			void				_Scan();
			void				_Connect(BMessage* message, bool connect);
			void				_Status(const char* text);

			BMessenger			fTarget;
			BMessageRunner*		fScanRunner;
};


#endif	// MIDI_WORKER_H
