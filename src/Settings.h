/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef SETTINGS_H
#define SETTINGS_H


#include <Point.h>
#include <Rect.h>
#include <String.h>

#include <map>


/*!	The app's saved state: window frame, zoom, box positions and the names
	the user gave to boxes and jacks. Stored as a flattened BMessage in the
	user settings directory. Anything missing or out of range falls back to
	the default.
*/
class Settings {
public:
								Settings();

			status_t			Load();
			status_t			Save() const;

			BRect				WindowFrame() const { return fWindowFrame; }
			void				SetWindowFrame(BRect frame)
									{ fWindowFrame = frame; }

			float				Zoom() const { return fZoom; }
			void				SetZoom(float zoom);

			bool				ShowAll() const { return fShowAll; }
			void				SetShowAll(bool showAll)
									{ fShowAll = showAll; }

			const char*			Alias(const BString& key) const;
			void				SetAlias(const BString& key,
									const BString& name);

			std::map<BString, BPoint>& Positions() { return fPositions; }
			const std::map<BString, BPoint>& Positions() const
									{ return fPositions; }

private:
			BRect				fWindowFrame;
			float				fZoom;
			bool				fShowAll;
			std::map<BString, BString> fAliases;
			std::map<BString, BPoint> fPositions;
};


#endif	// SETTINGS_H
