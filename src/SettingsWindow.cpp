/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "SettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <GridView.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MediaAddOn.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <StringView.h>

#include <string.h>

#include "GraphModel.h"
#include "Messages.h"


enum choice_action {
	kChoiceParameter,
	kChoiceOutput,
	kChoiceInput,
	kChoiceChannel
};

// multi_audio parameter ids, see MediaWorker.cpp
static const int32 kInputRateParameter = 1;
static const int32 kOutputRateParameter = 2;
static const int32 kBufferFramesParameter = 3;

static const bigtime_t kRestartWait = 4000000;


SettingsWindow::SettingsWindow(const BMessenger& worker,
	const BMessenger& mainWindow, BRect parentFrame)
	:
	BWindow(BRect(0, 0, 480, 300), "Renku Graph settings", B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS
			| B_CLOSE_ON_ESCAPE),
	fWorker(worker),
	fMainWindow(mainWindow),
	fBusy(false)
{
	fContent = new BGroupView(B_VERTICAL);
	fStatusView = new BStringView("status", "Reading devices" B_UTF8_ELLIPSIS);
	fStatusView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fStatusView->SetTruncation(B_TRUNCATE_END);

	BButton* restartButton = new BButton("restart", "Restart media services",
		new BMessage(kMsgRestartMedia));
	BButton* refreshButton = new BButton("refresh", "Refresh",
		new BMessage(kMsgSettingsRefresh));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fContent)
		.Add(fStatusView)
		.AddGroup(B_HORIZONTAL)
			.Add(restartButton)
			.AddGlue()
			.Add(refreshButton)
		.End();

	CenterIn(parentFrame);
	_Query();
}


bool
SettingsWindow::QuitRequested()
{
	fMainWindow.SendMessage(kMsgSettingsWindowGone);
	return true;
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSettingsRefresh:
			Activate();
			_Query();
			break;

		case kMsgDeviceInfo:
			fBusy = false;
			_Rebuild(message);
			break;

		case kMsgSettingsChoice:
			_Choice(message);
			break;

		case kMsgRestartMedia:
		{
			fWorker.SendMessage(kMsgRestartMedia);
			fStatusView->SetText("Restarting media services" B_UTF8_ELLIPSIS);
			BMessage refresh(kMsgSettingsRefresh);
			BMessageRunner::StartSending(BMessenger(this), &refresh,
				kRestartWait, 1);
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}


void
SettingsWindow::_Query()
{
	BMessage query(kMsgQueryDevices);
	query.AddMessenger("reply_to", BMessenger(this));
	fWorker.SendMessage(&query);
}


void
SettingsWindow::_Rebuild(BMessage* info)
{
	while (BView* child = fContent->ChildAt(0)) {
		child->RemoveSelf();
		delete child;
	}

	const char* result;
	if (info->FindString("result", &result) == B_OK)
		fStatusView->SetText(result);
	else
		fStatusView->SetText("");

	if (!info->GetBool("server_running", false)) {
		fContent->GroupLayout()->AddView(new BStringView("down",
			"Media services are not running."));
		ResizeToPreferred();
		return;
	}

	fContent->GroupLayout()->AddView(_DefaultsView(info));

	BMessage device;
	for (int32 i = 0; info->FindMessage("device", i, &device) == B_OK; i++)
		fContent->GroupLayout()->AddView(_DeviceView(&device));

	ResizeToPreferred();
}


static bool
same_dormant(const dormant_node_info& a, const dormant_node_info& b)
{
	return a.addon == b.addon && a.flavor_id == b.flavor_id;
}


static BMenuField*
dormant_field(BMessage* info, const char* name, const char* label,
	const char* listField, const char* currentField, int32 action)
{
	BPopUpMenu* menu = new BPopUpMenu("none");
	menu->SetLabelFromMarked(true);

	dormant_node_info current;
	bool haveCurrent = false;
	const void* data;
	ssize_t size;
	if (info->FindData(currentField, B_RAW_TYPE, &data, &size) == B_OK
		&& size == sizeof(current)) {
		current = *static_cast<const dormant_node_info*>(data);
		haveCurrent = true;
	}

	for (int32 i = 0; info->FindData(listField, B_RAW_TYPE, i, &data, &size)
			== B_OK; i++) {
		if (size != sizeof(dormant_node_info))
			continue;
		dormant_node_info node
			= *static_cast<const dormant_node_info*>(data);
		node.name[B_MEDIA_NAME_LENGTH - 1] = '\0';

		BMessage* message = new BMessage(kMsgSettingsChoice);
		message->AddInt32("action", action);
		message->AddData("dormant", B_RAW_TYPE, &node, sizeof(node));
		BMenuItem* item = new BMenuItem(node.name, message);
		if (haveCurrent && same_dormant(node, current))
			item->SetMarked(true);
		menu->AddItem(item);
	}

	return new BMenuField(name, label, menu);
}


BView*
SettingsWindow::_DefaultsView(BMessage* info)
{
	BMenuField* outputField = dormant_field(info, "output", "Audio output:",
		"output_dormant", "current_output", kChoiceOutput);
	BMenuField* inputField = dormant_field(info, "input", "Audio input:",
		"input_dormant", "current_input", kChoiceInput);

	BPopUpMenu* channelMenu = new BPopUpMenu("none");
	channelMenu->SetLabelFromMarked(true);
	int32 outputNode = info->GetInt32("output_node", -1);
	int32 currentChannel = info->GetInt32("current_channel", -1);
	int32 channelID;
	const char* channelName;
	int32 nextChannel = 1;
	for (int32 i = 0; info->FindInt32("channel_id", i, &channelID) == B_OK
		&& info->FindString("channel_name", i, &channelName) == B_OK; i++) {
		BMessage* message = new BMessage(kMsgSettingsChoice);
		message->AddInt32("action", kChoiceChannel);
		message->AddInt32("node", outputNode);
		message->AddInt32("in_id", channelID);
		BString label = bus_label("Playback",
			info->GetInt32("channel_count", i, 0), nextChannel, channelName);
		BMenuItem* item = new BMenuItem(label.String(), message);
		if (channelID == currentChannel)
			item->SetMarked(true);
		channelMenu->AddItem(item);
	}
	BMenuField* channelField = new BMenuField("channel", "Output channel:",
		channelMenu);
	channelField->SetToolTip("Which jack of the default output the system "
		"mixer plays to");

	BBox* box = new BBox("defaults");
	box->SetLabel("Default devices");

	BGridView* grid = new BGridView();
	BLayoutBuilder::Grid<>(grid)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddMenuField(outputField, 0, 0)
		.AddMenuField(channelField, 0, 1)
		.AddMenuField(inputField, 0, 2);
	box->AddChild(grid);
	return box;
}


BMenuField*
SettingsWindow::_ParameterField(BMessage* device, int32 parameterID,
	const char* label)
{
	BMessage parameter;
	for (int32 i = 0; device->FindMessage("parameter", i, &parameter) == B_OK;
			i++) {
		if (parameter.GetInt32("id", -1) != parameterID)
			continue;

		BPopUpMenu* menu = new BPopUpMenu("none");
		menu->SetLabelFromMarked(true);

		int32 current;
		bool haveCurrent = parameter.FindInt32("value", &current) == B_OK;
		int32 value;
		const char* name;
		for (int32 item = 0;
				parameter.FindInt32("item_value", item, &value) == B_OK
				&& parameter.FindString("item_name", item, &name) == B_OK;
				item++) {
			BMessage* message = new BMessage(kMsgSettingsChoice);
			message->AddInt32("action", kChoiceParameter);
			message->AddInt32("node", device->GetInt32("node", -1));
			message->AddInt32("parameter", parameterID);
			message->AddInt32("value", value);
			BMenuItem* menuItem = new BMenuItem(name, message);
			if (haveCurrent && value == current)
				menuItem->SetMarked(true);
			menu->AddItem(menuItem);
		}
		return new BMenuField(label, label, menu);
	}
	return NULL;
}


BView*
SettingsWindow::_DeviceView(BMessage* device)
{
	BBox* box = new BBox("device");
	box->SetLabel(device->GetString("name", "Device"));

	BGridView* grid = new BGridView();
	BLayoutBuilder::Grid<> builder(grid);
	builder.SetInsets(B_USE_DEFAULT_SPACING);
	int32 row = 0;

	// Only the rates of the directions the device has.
	BMenuField* field = NULL;
	if (device->GetBool("has_capture", true)) {
		field = _ParameterField(device, kInputRateParameter,
			"Input sample rate:");
		if (field != NULL)
			builder.AddMenuField(field, 0, row++);
	}
	if (device->GetBool("has_playback", true)) {
		field = _ParameterField(device, kOutputRateParameter,
			"Output sample rate:");
		if (field != NULL)
			builder.AddMenuField(field, 0, row++);
	}

	field = _ParameterField(device, kBufferFramesParameter, "Buffer size:");
	if (field == NULL) {
		BPopUpMenu* menu = new BPopUpMenu("Not available");
		field = new BMenuField("Buffer size:", "Buffer size:", menu);
		field->SetEnabled(false);
		field->SetToolTip("This device has no buffer size setting. RenkuOS "
			"offers it for USB audio interfaces, in versions that include "
			"the multi_audio buffer size change.");
	}
	builder.AddMenuField(field, 0, row++);

	int32 frames;
	float rate;
	if (device->FindInt32("period_frames", &frames) == B_OK
		&& device->FindFloat("frame_rate", &rate) == B_OK && rate > 0) {
		BString text;
		text.SetToFormat("%" B_PRId32 " frames, %.2f ms at %.0f Hz", frames,
			frames * 1000.0 / rate, rate);
		if (!device->GetBool("period_connected", false))
			text << " (not in use)";
		builder.Add(new BStringView("period_label", "Current buffer:"), 0,
			row);
		builder.Add(new BStringView("period", text.String()), 1, row++);
	}

	box->AddChild(grid);
	return box;
}


void
SettingsWindow::_Choice(BMessage* message)
{
	BMessage request;
	switch (message->GetInt32("action", -1)) {
		case kChoiceParameter:
			request.what = kMsgSetParameter;
			request.AddInt32("node", message->GetInt32("node", -1));
			request.AddInt32("parameter", message->GetInt32("parameter", -1));
			request.AddInt32("value", message->GetInt32("value", 0));
			break;

		case kChoiceOutput:
		case kChoiceInput:
		{
			const void* data;
			ssize_t size;
			if (message->FindData("dormant", B_RAW_TYPE, &data, &size)
					!= B_OK)
				return;
			request.what = message->GetInt32("action", -1) == kChoiceOutput
				? kMsgSetDefaultOutput : kMsgSetDefaultInput;
			request.AddData("dormant", B_RAW_TYPE, data, size);
			break;
		}

		case kChoiceChannel:
			request.what = kMsgSetDefaultChannel;
			request.AddInt32("node", message->GetInt32("node", -1));
			request.AddInt32("in_id", message->GetInt32("in_id", -1));
			break;

		default:
			return;
	}

	request.AddMessenger("reply_to", BMessenger(this));
	fBusy = true;
	fStatusView->SetText("Applying" B_UTF8_ELLIPSIS);
	fWorker.SendMessage(&request);
}
