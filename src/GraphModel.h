/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef GRAPH_MODEL_H
#define GRAPH_MODEL_H


#include <MediaDefs.h>
#include <MediaNode.h>
#include <Rect.h>
#include <String.h>

#include <memory>
#include <vector>


class Settings;
struct MediaSnapshot;
struct MidiSnapshot;
struct GraphBox;


enum port_kind {
	kAudioPort,
	kMidiPort,
	kOtherPort
};


struct GraphPort {
								GraphPort();

			GraphBox*			box;
			port_kind			kind;
			bool				isInput;
			bool				connected;
			BString				key;
				// stable name for aliases: box key + raw name
			BString				label;
			BString				rawName;
			BString				tip;

			// Media Kit endpoint (audio and other media)
			media_input			input;
			media_output		output;

			// Midi Kit endpoint
			int32				midiID;

			// geometry in canvas units, set by the view's layout
			BRect				frame;
			BPoint				dot;
};


enum box_column {
	kSourceColumn = 0,
	kMixedColumn,
	kSinkColumn
};


struct GraphBox {
								GraphBox();

			BString				key;
				// stable across restarts: kind + name (+ side)
			BString				title;
			BString				defaultTitle;
			BString				badge;
				// "default output" etc.
			BString				tip;
			bool				hardware;
			bool				controllable;
			media_node			node;
				// media_node::null for MIDI-only boxes
			std::vector<std::unique_ptr<GraphPort>> inputs;
			std::vector<std::unique_ptr<GraphPort>> outputs;

			BRect				frame;

			box_column			Column() const;
};


struct GraphWire {
			GraphPort*			from;
			GraphPort*			to;
};


/*!	The graph shown on the canvas: built from the latest media and MIDI
	snapshots each time either changes. All labelling happens here.
*/
class GraphModel {
public:
								GraphModel();

			void				Build(const MediaSnapshot* media,
									const MidiSnapshot* midi,
									const Settings& settings);

			bool				ServerRunning() const
									{ return fServerRunning; }

			std::vector<std::unique_ptr<GraphBox>>& Boxes() { return fBoxes; }
			std::vector<GraphWire>& Wires() { return fWires; }

private:
			void				_AddMediaNodes(const MediaSnapshot* media,
									const Settings& settings);
			void				_AddMidiEndpoints(const MidiSnapshot* midi,
									const Settings& settings);
			void				_AddWires(const MidiSnapshot* midi);
			GraphBox*			_NewBox(const BString& key,
									const BString& title,
									const Settings& settings);
			GraphPort*			_NewPort(GraphBox* box, bool isInput,
									port_kind kind, const BString& rawName,
									const BString& label,
									const Settings& settings);

			bool				fServerRunning;
			std::vector<std::unique_ptr<GraphBox>> fBoxes;
			std::vector<GraphWire> fWires;
};


BString					bus_label(const char* prefix, int32 channelCount,
							int32& nextChannel, const char* rawName);


#endif	// GRAPH_MODEL_H
