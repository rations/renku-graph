/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H


#include <Messenger.h>
#include <Window.h>


class BGroupView;
class BMenuField;
class BStringView;


/*!	Default audio devices, and per sound card: sample rates and buffer size.
	Changes apply at once, like in Media preferences. All media calls go
	through the media worker; its replies rebuild the window.
*/
class SettingsWindow : public BWindow {
public:
								SettingsWindow(const BMessenger& worker,
									const BMessenger& mainWindow,
									BRect parentFrame);

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_Query();
			void				_Rebuild(BMessage* info);
			BView*				_DefaultsView(BMessage* info);
			BView*				_DeviceView(BMessage* device);
			BMenuField*			_ParameterField(BMessage* device,
									int32 parameterID, const char* label);
			void				_Choice(BMessage* message);

			BMessenger			fWorker;
			BMessenger			fMainWindow;
			BGroupView*			fContent;
			BStringView*		fStatusView;
			bool				fBusy;
};


#endif	// SETTINGS_WINDOW_H
