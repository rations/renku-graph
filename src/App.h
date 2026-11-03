/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef APP_H
#define APP_H


#include <Application.h>

#include "Settings.h"


class App : public BApplication {
public:
								App();

	virtual	void				ReadyToRun();

private:
			Settings			fSettings;
};


#endif	// APP_H
