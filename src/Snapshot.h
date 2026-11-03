/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H


#include <MediaDefs.h>
#include <MediaNode.h>
#include <String.h>

#include <vector>


/*!	Plain copies of what the workers read from the media_server and the
	midi_server. The window builds its graph from these and never calls the
	rosters itself.
*/


struct MediaNodeEntry {
	media_node					node;
	BString						name;
	std::vector<media_input>	inputs;
	std::vector<media_output>	outputs;
};


struct MediaSnapshot {
								MediaSnapshot()
									:
									serverRunning(false),
									mixer(-1),
									audioOutput(-1),
									audioOutputInput(-1),
									audioInput(-1)
								{
								}

	bool						serverRunning;
	std::vector<MediaNodeEntry>	nodes;
	media_node_id				mixer;
	media_node_id				audioOutput;
	int32						audioOutputInput;
	media_node_id				audioInput;
};


struct MidiEndpointEntry {
	int32						id;
	BString						name;
	BString						device;
		// "device" property: set by midi_server for hardware ports
	bool						producer;
	std::vector<int32>			connections;
		// consumer ids, producers only
};


struct MidiSnapshot {
	std::vector<MidiEndpointEntry> endpoints;
};


#endif	// SNAPSHOT_H
