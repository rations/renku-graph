/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef RENAME_WINDOW_H
#define RENAME_WINDOW_H


#include <Messenger.h>
#include <String.h>
#include <Window.h>


class BTextControl;


/*!	Asks for a new name for a box or a jack. An empty name goes back to the
	name the system gives it.
*/
class RenameWindow : public BWindow {
public:
								RenameWindow(const BMessenger& target,
									const char* key, const char* current,
									const char* what, BRect parentFrame);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Send(const char* name);

			BMessenger			fTarget;
			BString				fKey;
			BTextControl*		fNameControl;
};


#endif	// RENAME_WINDOW_H
