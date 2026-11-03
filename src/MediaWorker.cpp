/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "MediaWorker.h"

#include <InterfaceDefs.h>
#include <MediaAddOn.h>
#include <MediaRoster.h>
#include <MessageRunner.h>
#include <ParameterWeb.h>

#include <utility>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Messages.h"
#include "Snapshot.h"


static const bigtime_t kScanDelay = 100000;
static const bigtime_t kRetryDelay = 2000000;
static const int32 kMaxEndpoints = 1024;
static const int32 kMaxNodes = 4096;

// multi_audio parameter ids (src/add-ons/media/media-add-ons/multi_audio/
// MultiAudioNode.cpp). The buffer size one only exists with the buffer size
// change applied, and only for USB devices. Their names are translated, so
// they are found by id.
static const int32 kInputRateParameter = 1;
static const int32 kOutputRateParameter = 2;
static const int32 kBufferFramesParameter = 3;


MediaWorker::MediaWorker(const BMessenger& target)
	:
	BLooper("media worker"),
	fTarget(target),
	fRoster(NULL),
	fScanRunner(NULL),
	fWatching(false),
	fWatchingServer(false)
{
}


MediaWorker::~MediaWorker()
{
	delete fScanRunner;
}


bool
MediaWorker::QuitRequested()
{
	if (fRoster != NULL) {
		if (fWatching)
			fRoster->StopWatching(BMessenger(this));
		if (fWatchingServer) {
			fRoster->StopWatching(BMessenger(this), B_MEDIA_SERVER_STARTED);
			fRoster->StopWatching(BMessenger(this), B_MEDIA_SERVER_QUIT);
		}
	}
	return true;
}


void
MediaWorker::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgWorkerStart:
			_Start();
			break;

		case kMsgScanTimer:
			delete fScanRunner;
			fScanRunner = NULL;
			_Scan();
			break;

		case kMsgScanNow:
			_Scan();
			break;

		case B_MEDIA_SERVER_STARTED:
			// The new media_server knows nothing of the old registration.
			fWatching = false;
			_Status("Media services started.");
			_ScheduleScan(kScanDelay);
			break;

		case B_MEDIA_SERVER_QUIT:
			fWatching = false;
			_Status("Media services stopped.");
			_ScheduleScan(kScanDelay);
			break;

		case B_MEDIA_NODE_CREATED:
		case B_MEDIA_NODE_DELETED:
		case B_MEDIA_CONNECTION_MADE:
		case B_MEDIA_CONNECTION_BROKEN:
		case B_MEDIA_FORMAT_CHANGED:
		case B_MEDIA_DEFAULT_CHANGED:
		case B_MEDIA_FLAVORS_CHANGED:
			_ScheduleScan(kScanDelay);
			break;

		case kMsgConnectAudio:
			_Connect(message);
			break;

		case kMsgDisconnectAudio:
			_Disconnect(message);
			break;

		case kMsgQueryDevices:
			_QueryDevices(message);
			break;

		case kMsgSetParameter:
			_SetParameter(message);
			break;

		case kMsgSetDefaultOutput:
		case kMsgSetDefaultInput:
		case kMsgSetDefaultChannel:
			_SetDefault(message);
			break;

		case kMsgRestartMedia:
			_Restart();
			break;

		default:
			BLooper::MessageReceived(message);
	}
}


void
MediaWorker::_Start()
{
	_Scan();
}


bool
MediaWorker::_EnsureRoster()
{
	if (fRoster == NULL) {
		status_t error;
		fRoster = BMediaRoster::Roster(&error);
		if (fRoster == NULL)
			return false;
	}

	if (!fWatchingServer) {
		BMessenger self(this);
		if (fRoster->StartWatching(self, B_MEDIA_SERVER_STARTED) == B_OK
			&& fRoster->StartWatching(self, B_MEDIA_SERVER_QUIT) == B_OK)
			fWatchingServer = true;
	}

	if (!fWatching && BMediaRoster::IsRunning()) {
		if (fRoster->StartWatching(BMessenger(this)) == B_OK)
			fWatching = true;
	}

	return true;
}


void
MediaWorker::_ScheduleScan(bigtime_t delay)
{
	if (fScanRunner != NULL)
		return;

	BMessage timer(kMsgScanTimer);
	fScanRunner = new(std::nothrow) BMessageRunner(BMessenger(this), &timer,
		delay, 1);
	if (fScanRunner != NULL && fScanRunner->InitCheck() != B_OK) {
		delete fScanRunner;
		fScanRunner = NULL;
	}
}


status_t
MediaWorker::_GetLiveNodes(std::vector<live_node_info>& nodes)
{
	// GetLiveNodes() fills at most the given count, so grow until it fits.
	for (int32 capacity = 64; capacity <= kMaxNodes; capacity *= 2) {
		nodes.resize(capacity);
		int32 count = capacity;
		status_t error = fRoster->GetLiveNodes(nodes.data(), &count);
		if (error != B_OK) {
			nodes.clear();
			return error;
		}
		if (count < capacity) {
			nodes.resize(count);
			return B_OK;
		}
	}
	return B_OK;
}


status_t
MediaWorker::_GetInputs(const media_node& node,
	std::vector<media_input>& inputs)
{
	inputs.clear();
	if ((node.kind & B_BUFFER_CONSUMER) == 0)
		return B_OK;

	for (int32 capacity = 16; capacity <= kMaxEndpoints; capacity *= 2) {
		inputs.resize(capacity);
		int32 count = 0;
		status_t error = fRoster->GetAllInputsFor(node, inputs.data(),
			capacity, &count);
		if (error != B_OK) {
			inputs.clear();
			return error;
		}
		if (count < capacity) {
			inputs.resize(count);
			return B_OK;
		}
	}
	return B_OK;
}


status_t
MediaWorker::_GetOutputs(const media_node& node,
	std::vector<media_output>& outputs)
{
	outputs.clear();
	if ((node.kind & B_BUFFER_PRODUCER) == 0)
		return B_OK;

	for (int32 capacity = 16; capacity <= kMaxEndpoints; capacity *= 2) {
		outputs.resize(capacity);
		int32 count = 0;
		status_t error = fRoster->GetAllOutputsFor(node, outputs.data(),
			capacity, &count);
		if (error != B_OK) {
			outputs.clear();
			return error;
		}
		if (count < capacity) {
			outputs.resize(count);
			return B_OK;
		}
	}
	return B_OK;
}


void
MediaWorker::_Scan()
{
	MediaSnapshot* snapshot = new(std::nothrow) MediaSnapshot;
	if (snapshot == NULL)
		return;

	if (_EnsureRoster() && BMediaRoster::IsRunning()) {
		snapshot->serverRunning = true;

		std::vector<live_node_info> live;
		_GetLiveNodes(live);
		for (size_t i = 0; i < live.size(); i++) {
			MediaNodeEntry entry;
			entry.node = live[i].node;
			entry.name.SetTo(live[i].name, B_MEDIA_NAME_LENGTH);
			_GetInputs(entry.node, entry.inputs);
			_GetOutputs(entry.node, entry.outputs);
			snapshot->nodes.push_back(entry);
		}

		media_node node;
		if (fRoster->GetAudioMixer(&node) == B_OK) {
			snapshot->mixer = node.node;
			fRoster->ReleaseNode(node);
		}
		int32 inputID = -1;
		BString inputName;
		if (fRoster->GetAudioOutput(&node, &inputID, &inputName) == B_OK) {
			snapshot->audioOutput = node.node;
			snapshot->audioOutputInput = inputID;
			fRoster->ReleaseNode(node);
		}
		if (fRoster->GetAudioInput(&node) == B_OK) {
			snapshot->audioInput = node.node;
			fRoster->ReleaseNode(node);
		}
	} else {
		// No media_server: try again until it shows up. Once the roster
		// exists, B_MEDIA_SERVER_STARTED also triggers a scan.
		_ScheduleScan(kRetryDelay);
	}

	BMessage message(kMsgMediaSnapshot);
	message.AddPointer("snapshot", snapshot);
	if (fTarget.SendMessage(&message) != B_OK)
		delete snapshot;
}


static void
endpoint_ids(BMessage* message, media_node_id* outNode, media_source* source,
	media_node_id* inNode, media_destination* destination)
{
	*outNode = message->GetInt32("out_node", -1);
	source->port = message->GetInt32("out_port", -1);
	source->id = message->GetInt32("out_id", -1);
	*inNode = message->GetInt32("in_node", -1);
	destination->port = message->GetInt32("in_port", -1);
	destination->id = message->GetInt32("in_id", -1);
}


void
MediaWorker::_Connect(BMessage* message)
{
	if (!_EnsureRoster())
		return;

	media_node_id outNodeID;
	media_node_id inNodeID;
	media_source source;
	media_destination destination;
	endpoint_ids(message, &outNodeID, &source, &inNodeID, &destination);

	// Look the endpoints up again: the window's copy can be stale, and the
	// free endpoint's current format is the connection template.
	media_node outNode;
	media_node inNode;
	if (fRoster->GetNodeFor(outNodeID, &outNode) != B_OK) {
		_Status("Could not connect: the source node is gone.");
		return;
	}
	if (fRoster->GetNodeFor(inNodeID, &inNode) != B_OK) {
		fRoster->ReleaseNode(outNode);
		_Status("Could not connect: the destination node is gone.");
		return;
	}

	std::vector<media_output> outputs;
	std::vector<media_input> inputs;
	_GetOutputs(outNode, outputs);
	_GetInputs(inNode, inputs);
	fRoster->ReleaseNode(outNode);
	fRoster->ReleaseNode(inNode);

	const media_output* output = NULL;
	for (size_t i = 0; i < outputs.size(); i++) {
		if (outputs[i].source == source)
			output = &outputs[i];
	}
	const media_input* input = NULL;
	for (size_t i = 0; i < inputs.size(); i++) {
		if (inputs[i].destination == destination)
			input = &inputs[i];
	}

	if (output == NULL || input == NULL) {
		_Status("Could not connect: that jack is gone.");
		return;
	}
	if (output->destination != media_destination::null) {
		_Status("\"%s\" is already connected. A Media Kit output takes one "
			"connection.", output->name);
		return;
	}
	if (input->source != media_source::null) {
		_Status("\"%s\" is already connected. A Media Kit input takes one "
			"connection.", input->name);
		return;
	}

	// Same template rule as Cortex (NodeManager::connect): the output's
	// format if it names a type, otherwise the input's.
	media_format format;
	if (output->format.type > B_MEDIA_UNKNOWN_TYPE) {
		format = output->format;
		if (input->format.type > B_MEDIA_UNKNOWN_TYPE
			&& input->format.type != format.type) {
			_Status("Could not connect \"%s\" to \"%s\": the media types "
				"differ.", output->name, input->name);
			return;
		}
	} else if (input->format.type > B_MEDIA_UNKNOWN_TYPE)
		format = input->format;
	else
		format.type = B_MEDIA_UNKNOWN_TYPE;

	media_output madeOutput;
	media_input madeInput;
	status_t error = fRoster->Connect(output->source, input->destination,
		&format, &madeOutput, &madeInput);
	if (error != B_OK) {
		_Status("Could not connect \"%s\" to \"%s\": %s", output->name,
			input->name, strerror(error));
		return;
	}

	_Status("Connected \"%s\" to \"%s\".", madeOutput.name, madeInput.name);
	_ScheduleScan(kScanDelay);
}


void
MediaWorker::_Disconnect(BMessage* message)
{
	if (!_EnsureRoster())
		return;

	media_node_id outNodeID;
	media_node_id inNodeID;
	media_source source;
	media_destination destination;
	endpoint_ids(message, &outNodeID, &source, &inNodeID, &destination);

	status_t error = fRoster->Disconnect(outNodeID, source, inNodeID,
		destination);
	if (error != B_OK)
		_Status("Could not disconnect: %s", strerror(error));
	else
		_Status("Disconnected.");

	_ScheduleScan(kScanDelay);
}


static void
add_discrete_parameter(BMessage& device, BParameterWeb* web, int32 id)
{
	for (int32 i = 0; i < web->CountParameters(); i++) {
		BParameter* parameter = web->ParameterAt(i);
		if (parameter == NULL || parameter->ID() != id
			|| parameter->Type() != BParameter::B_DISCRETE_PARAMETER)
			continue;

		BDiscreteParameter* discrete
			= static_cast<BDiscreteParameter*>(parameter);

		int32 value = 0;
		size_t size = sizeof(value);
		bigtime_t when;
		bool haveValue = discrete->GetValue(&value, &size, &when) == B_OK
			&& size == sizeof(value);

		BMessage entry;
		entry.AddInt32("id", id);
		if (haveValue)
			entry.AddInt32("value", value);
		for (int32 item = 0; item < discrete->CountItems(); item++) {
			const char* name = discrete->ItemNameAt(item);
			entry.AddInt32("item_value", discrete->ItemValueAt(item));
			entry.AddString("item_name", name != NULL ? name : "");
		}
		device.AddMessage("parameter", &entry);
		return;
	}
}


static void
add_dormant_nodes(BMediaRoster* roster, BMessage& reply, const char* field,
	uint64 kind)
{
	media_format format;
	format.type = B_MEDIA_RAW_AUDIO;

	// The same query as the Media preferences: outputs are consumers with an
	// audio input, inputs are producers with an audio output.
	dormant_node_info infos[64];
	int32 count = 64;
	status_t error;
	if ((kind & B_PHYSICAL_OUTPUT) != 0) {
		error = roster->GetDormantNodes(infos, &count, &format, NULL, NULL,
			kind);
	} else {
		error = roster->GetDormantNodes(infos, &count, NULL, &format, NULL,
			kind);
	}
	if (error != B_OK)
		return;

	for (int32 i = 0; i < count; i++)
		reply.AddData(field, B_RAW_TYPE, &infos[i], sizeof(infos[i]));
}


void
MediaWorker::_QueryDevices(BMessage* message)
{
	BMessenger replyTo;
	if (message->FindMessenger("reply_to", &replyTo) != B_OK)
		return;

	BMessage reply(kMsgDeviceInfo);
	const char* result;
	if (message->FindString("result", &result) == B_OK)
		reply.AddString("result", result);
	if (!_EnsureRoster() || !BMediaRoster::IsRunning()) {
		reply.AddBool("server_running", false);
		replyTo.SendMessage(&reply);
		return;
	}
	reply.AddBool("server_running", true);

	// default devices
	add_dormant_nodes(fRoster, reply, "output_dormant", B_PHYSICAL_OUTPUT);
	add_dormant_nodes(fRoster, reply, "input_dormant", B_PHYSICAL_INPUT);

	media_node node;
	dormant_node_info dormant;
	int32 channelID = -1;
	BString channelName;
	if (fRoster->GetAudioOutput(&node, &channelID, &channelName) == B_OK) {
		if (fRoster->GetDormantNodeFor(node, &dormant) == B_OK) {
			reply.AddData("current_output", B_RAW_TYPE, &dormant,
				sizeof(dormant));
		}
		reply.AddInt32("output_node", node.node);
		reply.AddInt32("current_channel", channelID);

		std::vector<media_input> inputs;
		_GetInputs(node, inputs);
		for (size_t i = 0; i < inputs.size(); i++) {
			reply.AddInt32("channel_id", inputs[i].destination.id);
			reply.AddString("channel_name", inputs[i].name);
			reply.AddInt32("channel_count",
				inputs[i].format.type == B_MEDIA_RAW_AUDIO
					? inputs[i].format.u.raw_audio.channel_count : 0);
		}
		fRoster->ReleaseNode(node);
	}
	if (fRoster->GetAudioInput(&node) == B_OK) {
		if (fRoster->GetDormantNodeFor(node, &dormant) == B_OK) {
			reply.AddData("current_input", B_RAW_TYPE, &dormant,
				sizeof(dormant));
		}
		fRoster->ReleaseNode(node);
	}

	// per-device parameters
	std::vector<live_node_info> live;
	_GetLiveNodes(live);
	for (size_t i = 0; i < live.size(); i++) {
		const media_node& liveNode = live[i].node;
		if ((liveNode.kind & (B_PHYSICAL_INPUT | B_PHYSICAL_OUTPUT)) == 0
			|| (liveNode.kind & B_CONTROLLABLE) == 0)
			continue;

		BMessage device;
		device.AddInt32("node", liveNode.node);
		device.AddString("name", BString(live[i].name, B_MEDIA_NAME_LENGTH));

		BParameterWeb* web = NULL;
		if (fRoster->GetParameterWebFor(liveNode, &web) == B_OK
			&& web != NULL) {
			add_discrete_parameter(device, web, kInputRateParameter);
			add_discrete_parameter(device, web, kOutputRateParameter);
			add_discrete_parameter(device, web, kBufferFramesParameter);
		}
		delete web;

		// The length the driver really uses: it can round the request.
		// Playback jacks first, capture jacks for capture-only devices.
		std::vector<media_input> inputs;
		std::vector<media_output> outputs;
		_GetInputs(liveNode, inputs);
		_GetOutputs(liveNode, outputs);
		device.AddBool("has_playback", !inputs.empty());
		device.AddBool("has_capture", !outputs.empty());

		std::vector<std::pair<media_format, bool>> formats;
		for (size_t j = 0; j < inputs.size(); j++) {
			formats.push_back(std::make_pair(inputs[j].format,
				inputs[j].source != media_source::null));
		}
		for (size_t j = 0; j < outputs.size(); j++) {
			formats.push_back(std::make_pair(outputs[j].format,
				outputs[j].destination != media_destination::null));
		}
		for (size_t j = 0; j < formats.size(); j++) {
			const media_format& format = formats[j].first;
			if (format.type != B_MEDIA_RAW_AUDIO)
				continue;
			const media_raw_audio_format& raw = format.u.raw_audio;
			size_t sampleSize = raw.format
				& media_raw_audio_format::B_AUDIO_SIZE_MASK;
			if (sampleSize == 0 || raw.channel_count == 0
				|| raw.buffer_size == 0 || raw.frame_rate <= 0)
				continue;
			device.AddInt32("period_frames",
				raw.buffer_size / (sampleSize * raw.channel_count));
			device.AddFloat("frame_rate", raw.frame_rate);
			device.AddBool("period_connected", formats[j].second);
			break;
		}

		reply.AddMessage("device", &device);
	}

	replyTo.SendMessage(&reply);
}


void
MediaWorker::_SetParameter(BMessage* message)
{
	if (!_EnsureRoster())
		return;

	media_node_id nodeID = message->GetInt32("node", -1);
	int32 parameterID = message->GetInt32("parameter", -1);
	int32 value = message->GetInt32("value", 0);

	media_node node;
	status_t error = fRoster->GetNodeFor(nodeID, &node);
	if (error != B_OK) {
		_Status("Could not change the setting: %s", strerror(error));
		return;
	}

	BParameterWeb* web = NULL;
	error = fRoster->GetParameterWebFor(node, &web);
	if (error == B_OK && web != NULL) {
		error = B_NAME_NOT_FOUND;
		for (int32 i = 0; i < web->CountParameters(); i++) {
			BParameter* parameter = web->ParameterAt(i);
			if (parameter != NULL && parameter->ID() == parameterID
				&& parameter->Type() == BParameter::B_DISCRETE_PARAMETER) {
				error = parameter->SetValue(&value, sizeof(value),
					system_time());
				break;
			}
		}
	}
	delete web;
	fRoster->ReleaseNode(node);

	BString result;
	if (error != B_OK)
		result.SetToFormat("Could not change the setting: %s",
			strerror(error));
	else
		result = "Setting changed.";
	_Status("%s", result.String());
	message->AddString("result", result);

	// The node applies the change asynchronously and sends no notification
	// for it, so give it a moment before reading the result back.
	snooze(300000);
	_QueryDevices(message);
}


void
MediaWorker::_SetDefault(BMessage* message)
{
	if (!_EnsureRoster())
		return;

	status_t error = B_BAD_VALUE;
	if (message->what == kMsgSetDefaultChannel) {
		media_node node;
		error = fRoster->GetNodeFor(message->GetInt32("node", -1), &node);
		if (error == B_OK) {
			int32 inputID = message->GetInt32("in_id", -1);
			std::vector<media_input> inputs;
			_GetInputs(node, inputs);
			error = B_NAME_NOT_FOUND;
			for (size_t i = 0; i < inputs.size(); i++) {
				if (inputs[i].destination.id == inputID) {
					error = fRoster->SetAudioOutput(inputs[i]);
					break;
				}
			}
			fRoster->ReleaseNode(node);
		}
	} else {
		const void* data;
		ssize_t size;
		if (message->FindData("dormant", B_RAW_TYPE, &data, &size) == B_OK
			&& size == sizeof(dormant_node_info)) {
			dormant_node_info info
				= *static_cast<const dormant_node_info*>(data);
			if (message->what == kMsgSetDefaultOutput)
				error = fRoster->SetAudioOutput(info);
			else
				error = fRoster->SetAudioInput(info);
		}
	}

	BString result;
	if (error != B_OK) {
		result.SetToFormat("Could not change the default device: %s",
			strerror(error));
	} else
		result = "Default device changed.";
	_Status("%s", result.String());
	message->AddString("result", result);

	snooze(300000);
	_QueryDevices(message);
}


void
MediaWorker::_Restart()
{
	_Status("Restarting media services" B_UTF8_ELLIPSIS);

	// Media preferences only calls shutdown_media_server(); launching
	// afterwards is a no-op when something else already started it again.
	status_t error = shutdown_media_server();
	if (error != B_OK)
		_Status("Could not stop media services: %s", strerror(error));

	error = launch_media_server();
	if (error != B_OK && error != B_ALREADY_RUNNING)
		_Status("Could not start media services: %s", strerror(error));

	fWatching = false;
	_ScheduleScan(kScanDelay);
}


void
MediaWorker::_Status(const char* format, ...)
{
	char text[512];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	BMessage message(kMsgStatus);
	message.AddString("text", text);
	fTarget.SendMessage(&message, (BHandler*)NULL, 1000000);
}
