/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "GraphModel.h"

#include <algorithm>
#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Settings.h"
#include "Snapshot.h"


GraphPort::GraphPort()
	:
	box(NULL),
	kind(kAudioPort),
	isInput(false),
	connected(false),
	hasFree(false),
	midiID(-1)
{
}


GraphBox::GraphBox()
	:
	hardware(false),
	controllable(false),
	node(media_node::null),
	split(0)
{
}


box_column
GraphBox::Column() const
{
	if (inputs.empty() && !outputs.empty())
		return kSourceColumn;
	if (outputs.empty() && !inputs.empty())
		return kSinkColumn;
	return kMixedColumn;
}


static port_kind
kind_for_format(const media_format& format)
{
	switch (format.type) {
		case B_MEDIA_RAW_AUDIO:
		case B_MEDIA_ENCODED_AUDIO:
		case B_MEDIA_UNKNOWN_TYPE:
			// A wildcard endpoint takes whatever it is offered; audio is
			// what this app routes, so treat it as audio.
			return kAudioPort;
		default:
			return kOtherPort;
	}
}


static BString
format_tip(const media_format& format)
{
	BString tip;
	char text[256];
	if (string_for_format(format, text, sizeof(text)))
		tip = text;

	if (format.type == B_MEDIA_RAW_AUDIO) {
		const media_raw_audio_format& raw = format.u.raw_audio;
		size_t sampleSize = raw.format
			& media_raw_audio_format::B_AUDIO_SIZE_MASK;
		if (sampleSize != 0 && raw.channel_count != 0 && raw.buffer_size != 0
			&& raw.frame_rate > 0) {
			uint32 frames = raw.buffer_size
				/ (sampleSize * raw.channel_count);
			BString period;
			period.SetToFormat("\n%" B_PRIu32 " frames per buffer, %.2f ms",
				frames, frames * 1000.0 / raw.frame_rate);
			tip << period;
		}
	}
	return tip;
}


/*!	"Playback 1+2" style label for a bus of a sound card, from its channel
	count. multi_audio publishes one jack per bus, numbered in channel order
	(MultiAudioNode::NodeRegistered()); \a nextChannel counts on from one bus
	to the next.
*/
BString
bus_label(const char* prefix, int32 channelCount, int32& nextChannel,
	const char* rawName)
{
	BString label;
	if (channelCount <= 0) {
		label = rawName;
		return label;
	}

	int32 first = nextChannel;
	int32 last = nextChannel + channelCount - 1;
	nextChannel += channelCount;

	if (channelCount == 1)
		label.SetToFormat("%s %" B_PRId32, prefix, first);
	else if (channelCount == 2)
		label.SetToFormat("%s %" B_PRId32 "+%" B_PRId32, prefix, first, last);
	else
		label.SetToFormat("%s %" B_PRId32 "-%" B_PRId32, prefix, first, last);
	return label;
}


static int32
channel_count(const media_format& format)
{
	if (format.type != B_MEDIA_RAW_AUDIO)
		return 0;
	return format.u.raw_audio.channel_count;
}


static int32
jack_id(const media_input& jack)
{
	return jack.destination.id;
}


static int32
jack_id(const media_output& jack)
{
	return jack.source.id;
}


static bool
jack_free(const media_input& jack)
{
	return jack.source == media_source::null;
}


static bool
jack_free(const media_output& jack)
{
	return jack.destination == media_destination::null;
}


/*!	Whether \a name is \a prefix followed by a number, which goes to
	\a _number.
*/
static bool
numbered_name(const BString& name, const char* prefix, int32& _number)
{
	size_t length = strlen(prefix);
	if (strncmp(name.String(), prefix, length) != 0)
		return false;

	const char* digits = name.String() + length;
	if (*digits < '0' || *digits > '9')
		return false;
	char* end;
	long number = strtol(digits, &end, 10);
	if (*end != '\0' || number < 0 || number > 100000)
		return false;
	_number = (int32)number;
	return true;
}


/*!	The jacks of one name on one side of a box: one row on the canvas. */
template<typename Jack>
struct JackRow {
	BString						name;
	std::vector<const Jack*>	jacks;
	BString						label;
	int64						order;
	bool						pair;
		// a bus of two or more channels
	bool						single;
		// one channel of a sound card that has a jack per channel
};


/*!	Whether \a row is shown. A sound card that has a jack per channel also
	has its buses (the system mixer plays stereo into "Playback 1+2"); unless
	the user asks for them, a bus is shown only while something is connected
	to it. A card with buses only shows them all.
*/
template<typename Jack>
static bool
row_shown(const JackRow<Jack>& row, const std::vector<JackRow<Jack>>& rows,
	bool showPairs)
{
	if (showPairs || !row.pair)
		return true;
	for (size_t j = 0; j < row.jacks.size(); j++) {
		if (!jack_free(*row.jacks[j]))
			return true;
	}
	for (size_t i = 0; i < rows.size(); i++) {
		if (rows[i].single)
			return false;
	}
	return true;
}


/*!	Groups \a jacks (sorted by id) into rows by name and labels them. A sound
	card's buses ("output 0", multi_audio's names, one per bus in channel
	order) become "Playback 1+2"; its single channels ("playback 3", from a
	multi_audio that offers them) become "Playback 3". Rows are ordered by
	channel, each bus before its channels; anything else keeps its name and
	comes last, in id order.
*/
template<typename Jack>
static std::vector<JackRow<Jack>>
jack_rows(const std::vector<const Jack*>& jacks, bool hardware,
	const char* busPrefix, const char* channelPrefix, const char* labelPrefix)
{
	std::vector<JackRow<Jack>> rows;
	std::map<BString, size_t> byName;
	for (size_t i = 0; i < jacks.size(); i++) {
		BString name(jacks[i]->name, B_MEDIA_NAME_LENGTH);
		auto found = byName.find(name);
		if (found == byName.end()) {
			byName[name] = rows.size();
			JackRow<Jack> row;
			row.name = name;
			row.pair = false;
			row.single = false;
			rows.push_back(row);
			found = byName.find(name);
		}
		rows[found->second].jacks.push_back(jacks[i]);
	}

	const int64 kLast = (int64)1 << 40;
	int32 nextChannel = 1;
	for (size_t i = 0; i < rows.size(); i++) {
		JackRow<Jack>& row = rows[i];
		row.label = row.name;
		row.order = kLast + (int64)i;
		if (!hardware)
			continue;

		int32 number;
		if (numbered_name(row.name, busPrefix, number)) {
			int32 count = channel_count(row.jacks[0]->format);
			int32 first = nextChannel;
			row.label = bus_label(labelPrefix, count, nextChannel,
				row.name.String());
			if (count > 0)
				row.order = (int64)first * 1024 + 1023 - count;
			row.pair = count >= 2;
		} else if (numbered_name(row.name, channelPrefix, number)
			&& number > 0) {
			row.label.SetToFormat("%s %" B_PRId32, labelPrefix, number);
			row.order = (int64)number * 1024 + 1023 - 1;
			row.single = true;
		}
	}

	std::stable_sort(rows.begin(), rows.end(),
		[](const JackRow<Jack>& a, const JackRow<Jack>& b) {
			return a.order < b.order;
		});
	return rows;
}


/*!	midi_server names a port by its device path unless the driver reports a
	name (DeviceWatcher::_SetProperties()); make the path readable.
*/
static BString
pretty_midi_name(const BString& name)
{
	static const char* kDevicePrefix = "/dev/midi/";
	if (!name.StartsWith(kDevicePrefix))
		return name;

	BString rest(name.String() + strlen(kDevicePrefix));
	BString pretty;
	if (rest.StartsWith("usb/"))
		pretty << "USB MIDI " << (rest.String() + 4);
	else
		pretty << "MIDI " << rest;
	return pretty;
}


GraphModel::GraphModel()
	:
	fServerRunning(false)
{
}


void
GraphModel::Build(const MediaSnapshot* media, const MidiSnapshot* midi,
	const Settings& settings)
{
	fBoxes.clear();
	fWires.clear();
	fServerRunning = media != NULL && media->serverRunning;

	if (media != NULL)
		_AddMediaNodes(media, settings);
	if (midi != NULL)
		_AddMidiEndpoints(midi, settings);

	_AddWires(midi);

	// Drop boxes that ended up without jacks.
	fBoxes.erase(std::remove_if(fBoxes.begin(), fBoxes.end(),
		[](const std::unique_ptr<GraphBox>& box) {
			return box->inputs.empty() && box->outputs.empty();
		}), fBoxes.end());
}


GraphBox*
GraphModel::_NewBox(const BString& key, const BString& title,
	const Settings& settings)
{
	GraphBox* box = new GraphBox;
	box->key = key;
	box->defaultTitle = title;
	const char* alias = settings.Alias(key);
	box->title = alias != NULL ? BString(alias) : title;
	fBoxes.push_back(std::unique_ptr<GraphBox>(box));
	return box;
}


GraphPort*
GraphModel::_NewPort(GraphBox* box, bool isInput, port_kind kind,
	const BString& rawName, const BString& label, const Settings& settings)
{
	GraphPort* port = new GraphPort;
	port->box = box;
	port->isInput = isInput;
	port->kind = kind;
	port->rawName = rawName;
	port->key << box->key << (isInput ? "/in:" : "/out:") << rawName;

	const char* alias = settings.Alias(port->key);
	port->label = alias != NULL ? BString(alias) : label;
	if (port->label.IsEmpty())
		port->label = isInput ? "Input" : "Output";

	if (isInput)
		box->inputs.push_back(std::unique_ptr<GraphPort>(port));
	else
		box->outputs.push_back(std::unique_ptr<GraphPort>(port));
	return port;
}


void
GraphModel::_AddMediaNodes(const MediaSnapshot* media,
	const Settings& settings)
{
	std::map<BString, int32> nameCount;

	for (size_t n = 0; n < media->nodes.size(); n++) {
		const MediaNodeEntry& entry = media->nodes[n];

		// Keep only what is shown: audio, or everything with "Show all".
		std::vector<const media_input*> inputs;
		std::vector<const media_output*> outputs;
		for (size_t i = 0; i < entry.inputs.size(); i++) {
			if (settings.ShowAll()
				|| kind_for_format(entry.inputs[i].format) == kAudioPort)
				inputs.push_back(&entry.inputs[i]);
		}
		for (size_t i = 0; i < entry.outputs.size(); i++) {
			if (settings.ShowAll()
				|| kind_for_format(entry.outputs[i].format) == kAudioPort)
				outputs.push_back(&entry.outputs[i]);
		}
		if (inputs.empty() && outputs.empty() && !settings.ShowAll())
			continue;

		std::sort(inputs.begin(), inputs.end(),
			[](const media_input* a, const media_input* b) {
				return jack_id(*a) < jack_id(*b);
			});
		std::sort(outputs.begin(), outputs.end(),
			[](const media_output* a, const media_output* b) {
				return jack_id(*a) < jack_id(*b);
			});

		// Node ids change when media services restart, names do not.
		BString baseKey("media:");
		baseKey << entry.name;
		int32 count = ++nameCount[entry.name];
		if (count > 1)
			baseKey << "#" << count;

		const uint32 kind = entry.node.kind;
		bool hardware = (kind & (B_PHYSICAL_INPUT | B_PHYSICAL_OUTPUT)) != 0;
		bool controllable = (kind & B_CONTROLLABLE) != 0;

		BString nodeTip;
		nodeTip << entry.name << "\nnode " << entry.node.node;

		GraphBox* inputBox;
		GraphBox* outputBox;
		if (hardware) {
			// A sound card is shown as two boxes, so that the signal flows
			// left to right: what it records on the left, what it plays on
			// the right.
			BString title(entry.name);
			title << " playback";
			BString key(baseKey);
			key << ":playback";
			inputBox = _NewBox(key, title, settings);

			title = entry.name;
			title << " capture";
			key = baseKey;
			key << ":capture";
			outputBox = _NewBox(key, title, settings);
		} else {
			inputBox = outputBox = _NewBox(baseKey, entry.name, settings);
		}

		GraphBox* boxes[2] = { inputBox, outputBox };
		for (int32 i = 0; i < 2; i++) {
			GraphBox* box = boxes[i];
			box->hardware = hardware;
			box->controllable = controllable;
			box->node = entry.node;
			box->tip = nodeTip;
		}

		if (entry.node.node == media->mixer)
			inputBox->badge = "system mixer";
		if (entry.node.node == media->audioOutput)
			inputBox->badge = "default output";
		if (entry.node.node == media->audioInput) {
			if (outputBox->badge.IsEmpty())
				outputBox->badge = "default input";
			else
				outputBox->badge << ", default input";
		}

		std::vector<JackRow<media_input>> inputRows = jack_rows(inputs,
			hardware, "output ", "playback ", "Playback");
		for (size_t r = 0; r < inputRows.size(); r++) {
			const JackRow<media_input>& row = inputRows[r];
			if (!row_shown(row, inputRows, settings.ShowPairs()))
				continue;
			GraphPort* port = _NewPort(inputBox, true,
				kind_for_format(row.jacks[0]->format), row.name, row.label,
				settings);
			bool defaultChannel = false;
			for (size_t j = 0; j < row.jacks.size(); j++) {
				const media_input& input = *row.jacks[j];
				port->inputJacks.push_back(input);
				if (!jack_free(input))
					port->connected = true;
				else if (!port->hasFree) {
					port->input = input;
					port->hasFree = true;
				}
				if (entry.node.node == media->audioOutput
					&& input.destination.id == media->audioOutputInput)
					defaultChannel = true;
			}
			if (!port->hasFree)
				port->input = *row.jacks[0];

			port->tip << row.name;
			if (defaultChannel)
				port->tip << " (default output channel)";
			BString format = format_tip(row.jacks[0]->format);
			if (!format.IsEmpty())
				port->tip << "\n" << format;
		}

		std::vector<JackRow<media_output>> outputRows = jack_rows(outputs,
			hardware, "input ", "capture ", "Capture");
		for (size_t r = 0; r < outputRows.size(); r++) {
			const JackRow<media_output>& row = outputRows[r];
			if (!row_shown(row, outputRows, settings.ShowPairs()))
				continue;
			GraphPort* port = _NewPort(outputBox, false,
				kind_for_format(row.jacks[0]->format), row.name, row.label,
				settings);
			for (size_t j = 0; j < row.jacks.size(); j++) {
				const media_output& output = *row.jacks[j];
				port->outputJacks.push_back(output);
				if (!jack_free(output))
					port->connected = true;
				else if (!port->hasFree) {
					port->output = output;
					port->hasFree = true;
				}
			}
			if (!port->hasFree)
				port->output = *row.jacks[0];

			port->tip << row.name;
			BString format = format_tip(row.jacks[0]->format);
			if (!format.IsEmpty())
				port->tip << "\n" << format;
		}
	}
}


void
GraphModel::_AddMidiEndpoints(const MidiSnapshot* midi,
	const Settings& settings)
{
	// Application endpoints go into the box of the application's media node
	// when their name starts with the node's name ("JackDAW MIDI in 1" joins
	// the "JackDAW" box).
	std::vector<GraphBox*> appBoxes;
	for (size_t i = 0; i < fBoxes.size(); i++) {
		if (!fBoxes[i]->hardware)
			appBoxes.push_back(fBoxes[i].get());
	}

	std::map<BString, GraphBox*> midiBoxes;

	for (size_t i = 0; i < midi->endpoints.size(); i++) {
		const MidiEndpointEntry& entry = midi->endpoints[i];
		bool isInput = !entry.producer;
			// a consumer receives MIDI: it is an input on the canvas

		GraphBox* box = NULL;
		BString label;

		if (!entry.device.IsEmpty()) {
			BString pretty = pretty_midi_name(entry.name);
			BString key("midi:");
			key << entry.name << (isInput ? ":playback" : ":capture");
			auto found = midiBoxes.find(key);
			if (found != midiBoxes.end())
				box = found->second;
			else {
				BString title(pretty);
				title << (isInput ? " playback" : " capture");
				box = _NewBox(key, title, settings);
				box->hardware = true;
				box->tip << entry.device;
				midiBoxes[key] = box;
			}
			label = "MIDI";
		} else {
			size_t bestLength = 0;
			for (size_t b = 0; b < appBoxes.size(); b++) {
				const BString& name = appBoxes[b]->defaultTitle;
				if (name.Length() > 0 && (size_t)name.Length() > bestLength
					&& entry.name.Length() > name.Length()
					&& entry.name.StartsWith(name)
					&& entry.name[name.Length()] == ' ') {
					box = appBoxes[b];
					bestLength = name.Length();
				}
			}

			if (box != NULL)
				label = entry.name.String() + bestLength + 1;
			else {
				BString key("midi:");
				key << entry.name;
				auto found = midiBoxes.find(key);
				if (found != midiBoxes.end())
					box = found->second;
				else {
					box = _NewBox(key, entry.name, settings);
					midiBoxes[key] = box;
				}
				label = isInput ? "MIDI in" : "MIDI out";
			}
		}

		BString raw("midi:");
		raw << entry.name;
		GraphPort* port = _NewPort(box, isInput, kMidiPort, raw, label,
			settings);
		port->midiID = entry.id;
		port->tip << entry.name << "\nMIDI endpoint " << entry.id;
		if (!entry.device.IsEmpty())
			port->tip << "\n" << entry.device;
	}
}


void
GraphModel::_AddWires(const MidiSnapshot* midi)
{
	std::vector<GraphPort*> mediaOutputs;
	std::map<int32, GraphPort*> producers;
	std::map<int32, GraphPort*> consumers;

	for (size_t b = 0; b < fBoxes.size(); b++) {
		GraphBox* box = fBoxes[b].get();
		for (size_t i = 0; i < box->outputs.size(); i++) {
			GraphPort* port = box->outputs[i].get();
			if (port->kind == kMidiPort)
				producers[port->midiID] = port;
			else if (port->connected)
				mediaOutputs.push_back(port);
		}
		for (size_t i = 0; i < box->inputs.size(); i++) {
			GraphPort* port = box->inputs[i].get();
			if (port->kind == kMidiPort)
				consumers[port->midiID] = port;
		}
	}

	// Media Kit: every connected input jack names its source.
	for (size_t b = 0; b < fBoxes.size(); b++) {
		GraphBox* box = fBoxes[b].get();
		for (size_t i = 0; i < box->inputs.size(); i++) {
			GraphPort* to = box->inputs[i].get();
			if (to->kind == kMidiPort || !to->connected)
				continue;
			for (size_t j = 0; j < to->inputJacks.size(); j++) {
				const media_input& input = to->inputJacks[j];
				if (jack_free(input))
					continue;
				bool found = false;
				for (size_t o = 0; o < mediaOutputs.size() && !found; o++) {
					GraphPort* from = mediaOutputs[o];
					for (size_t k = 0; k < from->outputJacks.size(); k++) {
						const media_output& output = from->outputJacks[k];
						if (output.source == input.source
							&& output.destination == input.destination) {
							fWires.push_back(GraphWire{from, to, output,
								input});
							found = true;
							break;
						}
					}
				}
			}
		}
	}

	if (midi == NULL)
		return;

	for (size_t i = 0; i < midi->endpoints.size(); i++) {
		const MidiEndpointEntry& entry = midi->endpoints[i];
		if (!entry.producer)
			continue;
		auto from = producers.find(entry.id);
		if (from == producers.end())
			continue;
		for (size_t c = 0; c < entry.connections.size(); c++) {
			auto to = consumers.find(entry.connections[c]);
			if (to == consumers.end())
				continue;
			from->second->connected = true;
			to->second->connected = true;
			fWires.push_back(GraphWire{from->second, to->second,
				media_output(), media_input()});
		}
	}
}
