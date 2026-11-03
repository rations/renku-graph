/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "App.h"

#include "MainWindow.h"
#include "Messages.h"


App::App()
	:
	BApplication(kAppSignature)
{
	fSettings.Load();
}


void
App::ReadyToRun()
{
	MainWindow* window = new MainWindow(fSettings);
	window->Show();
}


int
main()
{
	App app;
	app.Run();
	return 0;
}
