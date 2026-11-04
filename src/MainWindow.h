/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H


#include <Messenger.h>
#include <Window.h>

#include <memory>

#include "GraphModel.h"
#include "Settings.h"


class BMenuItem;
class BStringView;
class GraphView;
class MediaWorker;
class MidiWorker;
struct MediaSnapshot;
struct MidiSnapshot;


class MainWindow : public BWindow {
public:
								MainWindow(Settings& settings);
	virtual						~MainWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_Rebuild();
			void				_OpenSettings();
			void				_OpenParameters(BMessage* message);
			void				_Rename(BMessage* message);
			void				_RenameDone(BMessage* message);
			void				_ShowAbout();

			Settings&			fSettings;
			GraphModel			fModel;
			GraphView*			fGraphView;
			BStringView*		fStatusView;
			BMenuItem*			fShowAllItem;
			BMenuItem*			fShowPairsItem;

			MediaWorker*		fMediaWorker;
			MidiWorker*			fMidiWorker;
			std::unique_ptr<MediaSnapshot> fMedia;
			std::unique_ptr<MidiSnapshot> fMidi;

			BMessenger			fSettingsWindow;
};


#endif	// MAIN_WINDOW_H
