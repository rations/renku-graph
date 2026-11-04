/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollBar.h>
#include <StringView.h>

#include "GraphView.h"
#include "MediaWorker.h"
#include "Messages.h"
#include "MidiWorker.h"
#include "ParameterWindow.h"
#include "RenameWindow.h"
#include "SettingsWindow.h"
#include "Snapshot.h"


MainWindow::MainWindow(Settings& settings)
	:
	BWindow(settings.WindowFrame(), "Renku Graph", B_DOCUMENT_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fSettings(settings),
	fMediaWorker(NULL),
	fMidiWorker(NULL)
{
	BMenuBar* menuBar = new BMenuBar("menu");

	BMenu* menu = new BMenu("File");
	menu->AddItem(new BMenuItem("Settings" B_UTF8_ELLIPSIS,
		new BMessage(kMsgOpenSettings), ','));
	menu->AddItem(new BMenuItem("Restart media services",
		new BMessage(kMsgRestartMedia)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("About Renku Graph" B_UTF8_ELLIPSIS,
		new BMessage(B_ABOUT_REQUESTED)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(menu);

	menu = new BMenu("View");
	menu->AddItem(new BMenuItem("Zoom in", new BMessage(kMsgZoomIn), '+'));
	menu->AddItem(new BMenuItem("Zoom out", new BMessage(kMsgZoomOut), '-'));
	menu->AddItem(new BMenuItem("Actual size", new BMessage(kMsgZoomNormal),
		'0'));
	menu->AddItem(new BMenuItem("Fit to window", new BMessage(kMsgFitToWindow),
		'F'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Reset layout",
		new BMessage(kMsgResetLayout)));
	fShowAllItem = new BMenuItem("Show all nodes",
		new BMessage(kMsgToggleShowAll));
	fShowAllItem->SetMarked(fSettings.ShowAll());
	menu->AddItem(fShowAllItem);
	fShowPairsItem = new BMenuItem("Show stereo pairs",
		new BMessage(kMsgToggleShowPairs));
	fShowPairsItem->SetMarked(fSettings.ShowPairs());
	menu->AddItem(fShowPairsItem);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Refresh", new BMessage(kMsgRefresh), 'R'));
	menuBar->AddItem(menu);

	fGraphView = new GraphView(fModel, fSettings);
	fStatusView = new BStringView("status", "Looking for media services"
		B_UTF8_ELLIPSIS);
	fStatusView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fStatusView->SetTruncation(B_TRUNCATE_END);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(fGraphView, 1.0f)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2)
			.Add(fStatusView)
			.AddStrut(B_H_SCROLL_BAR_HEIGHT)
		.End();

	fMediaWorker = new MediaWorker(BMessenger(this));
	fMediaWorker->Run();
	fMediaWorker->PostMessage(kMsgWorkerStart);

	fMidiWorker = new MidiWorker(BMessenger(this));
	fMidiWorker->Run();
	fMidiWorker->PostMessage(kMsgWorkerStart);
}


MainWindow::~MainWindow()
{
	// The workers quit on their own; their messages to this window fail
	// from now on, and they delete what they could not send.
	if (fMediaWorker != NULL)
		fMediaWorker->PostMessage(B_QUIT_REQUESTED);
	if (fMidiWorker != NULL)
		fMidiWorker->PostMessage(B_QUIT_REQUESTED);
}


bool
MainWindow::QuitRequested()
{
	fSettings.SetWindowFrame(Frame());
	fSettings.SetZoom(fGraphView->Zoom());
	fSettings.Save();

	if (fSettingsWindow.IsValid())
		fSettingsWindow.SendMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgMediaSnapshot:
		{
			MediaSnapshot* snapshot;
			if (message->FindPointer("snapshot", (void**)&snapshot) == B_OK) {
				bool wasRunning = fMedia != NULL && fMedia->serverRunning;
				fMedia.reset(snapshot);
				if (!snapshot->serverRunning)
					fStatusView->SetText("Media services are not running.");
				else if (!wasRunning)
					fStatusView->SetText("Ready.");
				_Rebuild();
			}
			break;
		}

		case kMsgMidiSnapshot:
		{
			MidiSnapshot* snapshot;
			if (message->FindPointer("snapshot", (void**)&snapshot) == B_OK) {
				fMidi.reset(snapshot);
				_Rebuild();
			}
			break;
		}

		case kMsgStatus:
		{
			const char* text;
			if (message->FindString("text", &text) == B_OK)
				fStatusView->SetText(text);
			break;
		}

		case kMsgConnectAudio:
		case kMsgDisconnectAudio:
		case kMsgRestartMedia:
			fMediaWorker->PostMessage(message);
			break;

		case kMsgConnectMidi:
		case kMsgDisconnectMidi:
			fMidiWorker->PostMessage(message);
			break;

		case kMsgRefresh:
			fMediaWorker->PostMessage(kMsgScanNow);
			fMidiWorker->PostMessage(kMsgScanNow);
			break;

		case kMsgZoomIn:
			fGraphView->ZoomBy(1.2f);
			break;
		case kMsgZoomOut:
			fGraphView->ZoomBy(1 / 1.2f);
			break;
		case kMsgZoomNormal:
			fGraphView->SetZoom(1.0f);
			break;
		case kMsgFitToWindow:
			fGraphView->FitToWindow();
			break;
		case kMsgResetLayout:
			fGraphView->ResetLayout();
			break;

		case kMsgToggleShowAll:
			fSettings.SetShowAll(!fSettings.ShowAll());
			fShowAllItem->SetMarked(fSettings.ShowAll());
			_Rebuild();
			break;

		case kMsgToggleShowPairs:
			fSettings.SetShowPairs(!fSettings.ShowPairs());
			fShowPairsItem->SetMarked(fSettings.ShowPairs());
			_Rebuild();
			break;

		case kMsgOpenSettings:
			_OpenSettings();
			break;

		case kMsgSettingsWindowGone:
			fSettingsWindow = BMessenger();
			break;

		case kMsgOpenParameters:
			_OpenParameters(message);
			break;

		case kMsgRenameItem:
			_Rename(message);
			break;

		case kMsgRenameDone:
			_RenameDone(message);
			break;

		case B_ABOUT_REQUESTED:
			_ShowAbout();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
MainWindow::_Rebuild()
{
	fModel.Build(fMedia.get(), fMidi.get(), fSettings);
	fGraphView->ModelChanged();
}


void
MainWindow::_OpenSettings()
{
	if (fSettingsWindow.IsValid()) {
		// it activates itself and reads the devices again
		fSettingsWindow.SendMessage(kMsgSettingsRefresh);
		return;
	}

	SettingsWindow* window = new SettingsWindow(BMessenger(fMediaWorker),
		BMessenger(this), Frame());
	fSettingsWindow = BMessenger(window);
	window->Show();
}


void
MainWindow::_OpenParameters(BMessage* message)
{
	media_node_id node = message->GetInt32("node", -1);
	if (node < 0)
		return;

	BString title("Settings");
	for (size_t i = 0; i < fModel.Boxes().size(); i++) {
		GraphBox* box = fModel.Boxes()[i].get();
		if (box->node.node == node) {
			title = box->defaultTitle;
			if (box->hardware) {
				// "UMC204HD playback" -> the device name
				int32 space = title.FindLast(' ');
				if (space > 0)
					title.Truncate(space);
			}
			break;
		}
	}

	ParameterWindow* window = new ParameterWindow(node, title.String());
	window->Show();
}


void
MainWindow::_Rename(BMessage* message)
{
	const char* key;
	const char* current;
	const char* what;
	if (message->FindString("key", &key) != B_OK
		|| message->FindString("current", &current) != B_OK)
		return;
	if (message->FindString("what", &what) != B_OK)
		what = "item";

	RenameWindow* window = new RenameWindow(BMessenger(this), key, current,
		what, Frame());
	window->Show();
}


void
MainWindow::_RenameDone(BMessage* message)
{
	const char* key;
	const char* name;
	if (message->FindString("key", &key) != B_OK
		|| message->FindString("name", &name) != B_OK)
		return;

	BString trimmed(name);
	trimmed.Trim();
	if (trimmed.Length() > 200)
		trimmed.Truncate(200);
	fSettings.SetAlias(key, trimmed);
	fSettings.Save();
	_Rebuild();
}


void
MainWindow::_ShowAbout()
{
	BAlert* alert = new BAlert("About Renku Graph",
		"Renku Graph\n\n"
		"Audio and MIDI patchbay for RenkuOS.\n\n"
		"Drag from a jack to a jack to connect.\n"
		"Right-click a wire to disconnect it, or click a connected jack "
			"for a list of its wires.\n"
		"Double-click a box for its settings.\n\n"
		"Copyright 2026, rations. MIT License.",
		"OK");
	alert->Go(NULL);
}
