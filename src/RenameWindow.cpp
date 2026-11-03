/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "RenameWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#include "Messages.h"


enum {
	kMsgRenameOK		= 'rnOK',
	kMsgRenameReset		= 'rnRS'
};


RenameWindow::RenameWindow(const BMessenger& target, const char* key,
	const char* current, const char* what, BRect parentFrame)
	:
	BWindow(BRect(0, 0, 360, 100), "Rename", B_FLOATING_WINDOW_LOOK,
		B_MODAL_APP_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fKey(key)
{
	BString label("Name of this ");
	label << what << ":";

	fNameControl = new BTextControl("name", NULL, current, NULL);
	fNameControl->SetExplicitMinSize(BSize(300, B_SIZE_UNSET));

	BButton* okButton = new BButton("ok", "Rename",
		new BMessage(kMsgRenameOK));
	BButton* resetButton = new BButton("reset", "Use system name",
		new BMessage(kMsgRenameReset));
	BButton* cancelButton = new BButton("cancel", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(new BStringView("label", label.String()))
		.Add(fNameControl)
		.AddGroup(B_HORIZONTAL)
			.Add(resetButton)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
		.End();

	SetDefaultButton(okButton);
	fNameControl->MakeFocus(true);
	fNameControl->TextView()->SelectAll();

	CenterIn(parentFrame);
}


void
RenameWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgRenameOK:
			_Send(fNameControl->Text());
			break;

		case kMsgRenameReset:
			_Send("");
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
RenameWindow::_Send(const char* name)
{
	BMessage done(kMsgRenameDone);
	done.AddString("key", fKey);
	done.AddString("name", name);
	fTarget.SendMessage(&done);
	Quit();
}
